// Tauri requires this file; all real code lives in lib.rs.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    ptz_camera_control_lib::run();
}
