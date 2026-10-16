// Hitagi's desktop host extension; upstream guest and component state survive resize.
use super::*;

#[unsafe(no_mangle)]
pub extern "C" fn pocket_hitagi_resize(handle: *mut PocketApple, width: u32, height: u32) -> i32 {
    with_handle(handle, ERR_PANIC, |state| {
        if width == 0 || height == 0 || width > 8192 || height > 8192 {
            set_last_error("desktop viewport outside 1..8192");
            return ERR_BAD_ARGUMENT;
        }
        if width == state.logical_width && height == state.logical_height { return OK; }
        let length = if state.framebuffer.is_empty() { 0 } else {
            width as usize * height as usize * state.density as usize * state.density as usize * 4
        };
        if length > 256 * 1024 * 1024 {
            set_last_error("desktop surface exceeds 256 MiB");
            return ERR_BAD_ARGUMENT;
        }
        let mut pixels = Vec::new();
        if pixels.try_reserve_exact(length).is_err() {
            set_last_error("desktop surface allocation failed");
            return ERR_BAD_STATE;
        }
        pixels.resize(length, 0);
        if state.mounted {
            let script = format!("globalThis.__pocketResizeViewport?.({width},{height});");
            if let Err(error) = state.guest.eval("hitagi-resize", &script) {
                set_last_error(format!("resize hook failed: {error}"));
                return ERR_GUEST;
            }
        }
        state.surface.with_ui(|ui| ui.set_viewport(width as f32, height as f32));
        state.logical_width = width;
        state.logical_height = height;
        state.framebuffer = pixels;
        state.tracker.invalidate();
        OK
    })
}
