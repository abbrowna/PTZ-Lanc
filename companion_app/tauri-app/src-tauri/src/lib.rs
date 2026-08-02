//! UDP backend for the PTZ Camera Control Tauri companion app.
//!
//! All #[tauri::command] functions live in the `cmd` submodule to avoid
//! the "defined multiple times" conflict that arises when Tauri 2.x's
//! proc-macro and generate_handler! both emit __tauri_command_name_<X>
//! items in the same module scope.

use std::{
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

// ── Connection state (held in Tauri's managed state) ─────────────────────────

pub struct Conn {
    send_sock: Option<UdpSocket>,
    target:    Option<std::net::SocketAddr>,
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

pub type ConnState = Mutex<Conn>;

// ── Background receive thread ─────────────────────────────────────────────────

pub fn recv_loop(sock: UdpSocket, stop: Arc<AtomicBool>, app: AppHandle) {
    let mut buf = [0u8; 512];
    loop {
        if stop.load(Ordering::Relaxed) {
            break;
        }
        match sock.recv_from(&mut buf) {
            Ok((len, _src)) => {
                if let Ok(msg) = std::str::from_utf8(&buf[..len]) {
                    if let Some(update) = parse_status(msg.trim()) {
                        app.emit("status-update", update).ok();
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

pub fn parse_status(msg: &str) -> Option<StatusUpdate> {
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

// ── Tauri commands (isolated in their own module to prevent macro name clashes)
//
//   Tauri 2.x's #[tauri::command] proc-macro and generate_handler! both emit
//   module-level items named __tauri_command_name_<fn> and __cmd__<fn>.
//   Placing all commands in a dedicated submodule scopes those generated
//   symbols here, away from the generate_handler! call in run().

mod cmd {
    use std::{net::ToSocketAddrs, sync::atomic::Ordering};
    use tauri::{AppHandle, State};
    use super::{ConnState, recv_loop};

    #[tauri::command]
    pub fn camera_connect(
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
            .ok_or_else(|| format!("No address resolved for '{}'", host))?;

        let send_sock = std::net::UdpSocket::bind("0.0.0.0:0")
            .map_err(|e| e.to_string())?;
        let recv_sock = send_sock.try_clone().map_err(|e| e.to_string())?;
        recv_sock
            .set_read_timeout(Some(std::time::Duration::from_millis(100)))
            .map_err(|e| e.to_string())?;

        let stop_flag = std::sync::Arc::new(std::sync::atomic::AtomicBool::new(false));
        {
            let mut conn = state.lock().unwrap();
            conn.stop.store(true, Ordering::Relaxed);
            conn.send_sock = Some(send_sock);
            conn.target    = Some(target);
            conn.stop      = stop_flag.clone();
        }

        std::thread::spawn(move || recv_loop(recv_sock, stop_flag, app));

        Ok(target.to_string())
    }

    #[tauri::command]
    pub fn udp_send(msg: String, state: State<'_, ConnState>) -> Result<(), String> {
        let conn = state.lock().unwrap();
        match (&conn.send_sock, &conn.target) {
            (Some(sock), Some(target)) => {
                sock.send_to(msg.as_bytes(), target).map_err(|e| e.to_string())?;
                Ok(())
            }
            _ => Err("Not connected to camera".into()),
        }
    }

    #[tauri::command]
    pub fn udp_send_multi(msgs: Vec<String>, state: State<'_, ConnState>) -> Result<(), String> {
        let conn = state.lock().unwrap();
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
    pub fn camera_disconnect(state: State<'_, ConnState>) {
        let mut conn = state.lock().unwrap();
        conn.stop.store(true, Ordering::Relaxed);
        conn.send_sock = None;
        conn.target    = None;
    }
}

// ── App entry point ───────────────────────────────────────────────────────────

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(Mutex::new(Conn::new()))
        .invoke_handler(tauri::generate_handler![
            cmd::camera_connect,
            cmd::udp_send,
            cmd::udp_send_multi,
            cmd::camera_disconnect,
        ])
        .run(tauri::generate_context!())
        .expect("error running PTZ Camera Control");
}
