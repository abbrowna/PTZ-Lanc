//! UDP backend for the PTZ Camera Control Tauri companion app.
//!
//! Exposes three Tauri commands to the frontend:
//!   connect(host, port)  – resolve hostname, open socket, start receive thread
//!   udp_send(msg)        – fire-and-forget text packet to the camera
//!   disconnect()         – stop the receive thread and drop the socket
//!
//! The receive thread emits a "status-update" Tauri event whenever the
//! camera replies to a STATUS request.  The frontend listens for that
//! event instead of polling with fetch().

use std::{
    net::{ToSocketAddrs, UdpSocket},
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
    time::Duration,
};

use tauri::{AppHandle, Emitter, Manager, State};

// ── Shared status payload emitted to the frontend ────────────────────────────

#[derive(Clone, serde::Serialize)]
pub struct StatusUpdate {
    pub wb_k:     Option<i32>,
    pub exp_f:    Option<i32>,
    pub exp_s:    Option<i32>,
    pub exp_g:    Option<i32>,
    pub hostname: Option<String>,
}

// ── Connection state (held in Tauri's managed state) ─────────────────────────

pub struct Conn {
    /// Cloned socket used for sending (non-blocking is fine here).
    send_sock: Option<UdpSocket>,
    /// Resolved camera address.
    target:    Option<std::net::SocketAddr>,
    /// Signals the background receive thread to stop.
    stop:      Arc<AtomicBool>,
}

impl Conn {
    fn new() -> Self {
        Self {
            send_sock: None,
            target:    None,
            stop:      Arc::new(AtomicBool::new(false)),
        }
    }
}

// Tauri's managed state requires Send + Sync; Mutex<Conn> satisfies that.
pub type ConnState = Mutex<Conn>;

// ── Tauri commands ────────────────────────────────────────────────────────────

/// Resolve the hostname, open a UDP socket, and start the receive thread.
/// Returns the resolved address string on success.
#[tauri::command]
pub fn connect(
    host:  String,
    port:  u16,
    state: State<'_, ConnState>,
    app:   AppHandle,
) -> Result<String, String> {
    let addr_str = format!("{}:{}", host, port);
    let target   = addr_str
        .to_socket_addrs()
        .map_err(|e| format!("DNS error: {e}"))?
        .next()
        .ok_or_else(|| format!("No address found for '{}'", host))?;

    // Create the socket and clone it: one handle for sends, one for receives.
    let send_sock = UdpSocket::bind("0.0.0.0:0").map_err(|e| e.to_string())?;
    let recv_sock = send_sock.try_clone().map_err(|e| e.to_string())?;

    // The recv socket uses a short timeout so the thread can check the stop flag.
    recv_sock
        .set_read_timeout(Some(Duration::from_millis(100)))
        .map_err(|e| e.to_string())?;

    // Signal any previous receive thread to exit, then install new state.
    let stop_flag = Arc::new(AtomicBool::new(false));
    {
        let mut conn = state.lock().unwrap();
        conn.stop.store(true, Ordering::Relaxed); // stop old thread if any
        conn.send_sock = Some(send_sock);
        conn.target    = Some(target);
        conn.stop      = stop_flag.clone();
    }

    // Spawn background receive thread.
    std::thread::spawn(move || recv_loop(recv_sock, stop_flag, app));

    Ok(target.to_string())
}

/// Send a plain-text UDP control packet to the camera.
/// The protocol is documented in main.cpp's handleUDPControl().
#[tauri::command]
pub fn udp_send(msg: String, state: State<'_, ConnState>) -> Result<(), String> {
    let conn = state.lock().unwrap();
    match (&conn.send_sock, &conn.target) {
        (Some(sock), Some(target)) => {
            sock.send_to(msg.as_bytes(), target)
                .map_err(|e| e.to_string())?;
            Ok(())
        }
        _ => Err("Not connected to camera".into()),
    }
}

/// Send several packets at once – used for reliable "off" commands
/// (equivalent to the triple-send pattern in the original JS).
#[tauri::command]
pub fn udp_send_multi(msgs: Vec<String>, state: State<'_, ConnState>) -> Result<(), String> {
    let conn = state.lock().unwrap();
    match (&conn.send_sock, &conn.target) {
        (Some(sock), Some(target)) => {
            for msg in msgs {
                sock.send_to(msg.as_bytes(), target)
                    .map_err(|e| e.to_string())?;
            }
            Ok(())
        }
        _ => Err("Not connected to camera".into()),
    }
}

/// Stop the receive thread and drop the socket.
#[tauri::command]
pub fn disconnect(state: State<'_, ConnState>) {
    let mut conn = state.lock().unwrap();
    conn.stop.store(true, Ordering::Relaxed);
    conn.send_sock = None;
    conn.target    = None;
}

// ── Background receive thread ─────────────────────────────────────────────────

fn recv_loop(sock: UdpSocket, stop: Arc<AtomicBool>, app: AppHandle) {
    let mut buf = [0u8; 512];
    loop {
        if stop.load(Ordering::Relaxed) {
            break;
        }
        match sock.recv_from(&mut buf) {
            Ok((len, _src)) => {
                if let Ok(msg) = std::str::from_utf8(&buf[..len]) {
                    if let Some(update) = parse_status(msg.trim()) {
                        // Emit to all frontend windows; ignore error if window closed.
                        app.emit("status-update", update).ok();
                    }
                }
            }
            // recv timed out – normal, just loop and recheck the stop flag.
            Err(ref e)
                if e.kind() == std::io::ErrorKind::WouldBlock
                    || e.kind() == std::io::ErrorKind::TimedOut => {}
            // Real error (socket closed on disconnect) – exit thread.
            Err(_) => break,
        }
    }
}

// ── STATUS packet parser ──────────────────────────────────────────────────────
// Device sends: "STATUS wb_k=34 exp_f=8 exp_s=3 exp_g=17 hostname=camera-abc"

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

// ── App entry point ───────────────────────────────────────────────────────────

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(Mutex::new(Conn::new()))
        .invoke_handler(tauri::generate_handler![
            connect,
            udp_send,
            udp_send_multi,
            disconnect,
        ])
        .run(tauri::generate_context!())
        .expect("error running PTZ Camera Control");
}
