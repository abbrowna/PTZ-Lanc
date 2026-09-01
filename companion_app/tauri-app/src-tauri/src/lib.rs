//! UDP backend for the PTZ Camera Control Tauri companion app.
//!
//! Multi-window architecture:
//!   Each Tauri window gets its own independent UDP connection, keyed by the
//!   window's label in a shared HashMap.  Opening a second camera window from
//!   the "＋ New Camera Window" button creates a fresh WebviewWindow with a
//!   unique label; its camera_connect / udp_send calls automatically target
//!   its own Conn entry and never interfere with other open windows.
//!
//! Commands exposed to the frontend:
//!   camera_connect(host, port)  – DNS resolve, open socket, start recv thread
//!   udp_send(msg)               – fire-and-forget control packet
//!   udp_send_multi(msgs)        – N packets (reliable off-commands)
//!   camera_disconnect()         – stop recv thread, remove conn entry
//!   open_camera_window()        – spawn a new WebviewWindow for a second camera

use std::{
    collections::HashMap,
    net::UdpSocket,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
};

use tauri::{AppHandle, Emitter};

// ── Shared status payload emitted to the frontend ────────────────────────────

#[derive(Clone, serde::Serialize)]
pub struct StatusUpdate {
    pub wb_k:     Option<i32>,
    pub exp_f:    Option<i32>,
    pub exp_s:    Option<i32>,
    pub exp_g:    Option<i32>,
    pub hostname: Option<String>,
}

// ── Per-window connection state ───────────────────────────────────────────────

pub struct Conn {
    send_sock: Option<UdpSocket>,
    target:    Option<std::net::SocketAddr>,
    /// Signals the background recv thread to stop
    stop:      Arc<AtomicBool>,
}

/// Shared map: window_label → Conn
pub type ConnMap = Mutex<HashMap<String, Conn>>;

// ── Background receive thread ─────────────────────────────────────────────────
// Emits "status-update" only to the window identified by `label`.

fn recv_loop(
    sock:  UdpSocket,
    stop:  Arc<AtomicBool>,
    app:   AppHandle,
    label: String,
) {
    let mut buf = [0u8; 512];
    loop {
        if stop.load(Ordering::Relaxed) {
            break;
        }
        match sock.recv_from(&mut buf) {
            Ok((len, _src)) => {
                if let Ok(msg) = std::str::from_utf8(&buf[..len]) {
                    if let Some(update) = parse_status(msg.trim()) {
                        // Emit only to the window that owns this connection.
                        // NOTE: WebviewWindow::emit() is NOT window-scoped — it is
                        // equivalent to a global AppHandle::emit() and is delivered
                        // to every listener in every window. emit_to() is required
                        // to target a single window by label.
                        app.emit_to(label.as_str(), "status-update", update).ok();
                    }
                }
            }
            Err(ref e)
                if e.kind() == std::io::ErrorKind::WouldBlock
                    || e.kind() == std::io::ErrorKind::TimedOut => {}
            Err(_) => break,
        }
    }
}

// ── STATUS packet parser ──────────────────────────────────────────────────────

fn parse_status(msg: &str) -> Option<StatusUpdate> {
    if !msg.starts_with("STATUS ") {
        return None;
    }
    let mut u = StatusUpdate {
        wb_k: None, exp_f: None, exp_s: None, exp_g: None, hostname: None,
    };
    for part in msg[7..].split_ascii_whitespace() {
        if let Some((k, v)) = part.split_once('=') {
            match k {
                "wb_k"     => u.wb_k     = v.parse().ok(),
                "exp_f"    => u.exp_f    = v.parse().ok(),
                "exp_s"    => u.exp_s    = v.parse().ok(),
                "exp_g"    => u.exp_g    = v.parse().ok(),
                "hostname" => u.hostname = Some(v.to_string()),
                _ => {}
            }
        }
    }
    Some(u)
}

// ── Tauri commands ────────────────────────────────────────────────────────────
// All commands are in a submodule to avoid Tauri 2.x macro name-collision
// (see previous fix commit for full explanation).

mod cmd {
    use std::{net::ToSocketAddrs, sync::atomic::Ordering};
    use tauri::{AppHandle, State, WebviewWindow};
    use super::{ConnMap, Conn, recv_loop};

    /// Open a UDP socket to `host:port` and start the receive thread.
    /// The window's label is used as the key in the connection map so each
    /// window has a completely independent connection.
    #[tauri::command]
    pub fn camera_connect(
        host:   String,
        port:   u16,
        state:  State<'_, ConnMap>,
        window: WebviewWindow,   // auto-injected: the window that called invoke()
        app:    AppHandle,
    ) -> Result<String, String> {
        let label    = window.label().to_string();
        let addr_str = format!("{}:{}", host, port);
        let target   = addr_str
            .to_socket_addrs()
            .map_err(|e| format!("DNS error: {e}"))?
            .next()
            .ok_or_else(|| format!("No address resolved for '{}'", host))?;

        let send_sock = std::net::UdpSocket::bind("0.0.0.0:0")
            .map_err(|e| e.to_string())?;
        let recv_sock = send_sock.try_clone().map_err(|e| e.to_string())?;
        recv_sock
            .set_read_timeout(Some(std::time::Duration::from_millis(100)))
            .map_err(|e| e.to_string())?;

        let stop_flag = std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false));

        // Stop any previous connection for this window
        {
            let mut map = state.lock().unwrap();
            if let Some(old) = map.get(&label) {
                old.stop.store(true, Ordering::Relaxed);
            }
            map.insert(label.clone(), Conn {
                send_sock: Some(send_sock),
                target:    Some(target),
                stop:      stop_flag.clone(),
            });
        }

        // Spawn per-window recv thread
        let label_clone = label.clone();
        std::thread::spawn(move || recv_loop(recv_sock, stop_flag, app, label_clone));

        Ok(target.to_string())
    }

    #[tauri::command]
    pub fn udp_send(
        msg:    String,
        state:  State<'_, ConnMap>,
        window: WebviewWindow,
    ) -> Result<(), String> {
        let map  = state.lock().unwrap();
        let conn = map.get(window.label()).ok_or("Not connected to camera")?;
        match (&conn.send_sock, &conn.target) {
            (Some(sock), Some(target)) => {
                sock.send_to(msg.as_bytes(), target).map_err(|e| e.to_string())?;
                Ok(())
            }
            _ => Err("Not connected to camera".into()),
        }
    }

    #[tauri::command]
    pub fn udp_send_multi(
        msgs:   Vec<String>,
        state:  State<'_, ConnMap>,
        window: WebviewWindow,
    ) -> Result<(), String> {
        let map  = state.lock().unwrap();
        let conn = map.get(window.label()).ok_or("Not connected to camera")?;
        match (&conn.send_sock, &conn.target) {
            (Some(sock), Some(target)) => {
                for msg in msgs {
                    sock.send_to(msg.as_bytes(), target).map_err(|e| e.to_string())?;
                }
                Ok(())
            }
            _ => Err("Not connected to camera".into()),
        }
    }

    #[tauri::command]
    pub fn camera_disconnect(
        state:  State<'_, ConnMap>,
        window: WebviewWindow,
    ) {
        let mut map = state.lock().unwrap();
        if let Some(conn) = map.get(window.label()) {
            conn.stop.store(true, Ordering::Relaxed);
        }
        map.remove(window.label());
    }

    /// Open a new WebviewWindow pointing at the same UI.
    /// The new window gets a unique label (camera-<timestamp>) so it has
    /// its own independent connection slot in the ConnMap.
    #[tauri::command]
    pub fn open_camera_window(app: AppHandle) -> Result<(), String> {
        use tauri::WebviewWindowBuilder;
        let label = format!("camera-{}", std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis());

        WebviewWindowBuilder::new(&app, &label, tauri::WebviewUrl::App("/".into()))
            .title("PTZ Camera Control")
            .inner_size(1280.0, 760.0)
            .min_inner_size(900.0, 580.0)
            .resizable(true)
            .build()
            .map_err(|e| e.to_string())?;

        Ok(())
    }
}

// ── App entry point ───────────────────────────────────────────────────────────

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(Mutex::new(HashMap::<String, Conn>::new()))
        .invoke_handler(tauri::generate_handler![
            cmd::camera_connect,
            cmd::udp_send,
            cmd::udp_send_multi,
            cmd::camera_disconnect,
            cmd::open_camera_window,
        ])
        .run(tauri::generate_context!())
        .expect("error running PTZ Camera Control");
}
