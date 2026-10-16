//! Borrowed DrawList/resource views for Hitagi's renderer. No raster invocation.
use super::*;

#[repr(C)]
pub struct PocketHitagiDrawList {
    words: *const u32,
    word_count: usize,
    texture_slots: u32,
    font_slots: u32,
}
#[repr(C)]
pub struct PocketHitagiTexture {
    pixels: *const u8,
    byte_count: usize,
    palette: *const u8,
    palette_bytes: usize,
    revision: u64,
    handle: i32,
    width: u32,
    height: u32,
    format: u32,
    linear: u32,
}
#[repr(C)]
pub struct PocketHitagiFont {
    coverage: *const u8,
    byte_count: usize,
    revision: u64,
    cell_width: u32,
    cell_height: u32,
    coverage_width: u32,
    coverage_height: u32,
    glyph_count: u32,
}

#[unsafe(no_mangle)]
pub extern "C" fn pocket_hitagi_create_gpu(
    density: u32,
    width: u32,
    height: u32,
) -> *mut PocketApple {
    catch_unwind(AssertUnwindSafe(|| {
        if !(1..=4).contains(&density)
            || !(1..=8192).contains(&width)
            || !(1..=8192).contains(&height)
        {
            set_last_error("invalid GPU UI density/viewport");
            return std::ptr::null_mut();
        }
        let guest = match Guest::new() {
            Ok(guest) => guest,
            Err(error) => {
                set_last_error(error.to_string());
                return std::ptr::null_mut();
            }
        };
        Box::into_raw(Box::new(PocketApple {
            guest,
            surface: UiSurface::new_with_density((width as f32, height as f32), density),
            framebuffer: Vec::new(),
            tracker: DamageTracker::default(),
            density,
            logical_width: width,
            logical_height: height,
            mounted: false,
            effect_callback: None,
        }))
    }))
    .unwrap_or_else(|_| {
        set_last_error("GPU UI creation panicked");
        std::ptr::null_mut()
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn pocket_hitagi_draw(
    handle: *mut PocketApple,
    out: *mut PocketHitagiDrawList,
) -> i32 {
    with_handle(handle, ERR_PANIC, |state| {
        if out.is_null() || !state.mounted {
            return ERR_BAD_STATE;
        }
        state.surface.with_ui(|ui| {
            // draw may allocate glyph/disc texture pages; enumerate resources AFTER it.
            let (words, word_count) = {
                let list = ui.draw();
                (list.words.as_ptr(), list.words.len())
            };
            unsafe {
                *out = PocketHitagiDrawList {
                    words,
                    word_count,
                    texture_slots: ui.texture_slot_count() as u32,
                    font_slots: spec::MAX_FONT_SLOTS as u32,
                };
            }
        });
        OK
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn pocket_hitagi_texture(
    handle: *mut PocketApple,
    slot: u32,
    out: *mut PocketHitagiTexture,
) -> i32 {
    with_handle(handle, ERR_PANIC, |state| {
        if out.is_null() {
            return ERR_BAD_ARGUMENT;
        }
        state.surface.with_ui(|ui| {
            if slot as usize >= ui.texture_slot_count() {
                return ERR_BAD_ARGUMENT;
            }
            let Some((handle, revision, view)) = ui.texture_at_versioned(slot) else {
                return 1;
            };
            let palette = view.palette.unwrap_or(&[]);
            unsafe {
                *out = PocketHitagiTexture {
                    pixels: view.pixels.as_ptr(),
                    byte_count: view.pixels.len(),
                    palette: palette.as_ptr(),
                    palette_bytes: palette.len(),
                    revision,
                    handle,
                    width: view.w,
                    height: view.h,
                    format: view.psm,
                    linear: view.linear as u32,
                };
            }
            OK
        })
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn pocket_hitagi_font(
    handle: *mut PocketApple,
    slot: u32,
    out: *mut PocketHitagiFont,
) -> i32 {
    with_handle(handle, ERR_PANIC, |state| {
        if out.is_null() || slot as usize >= spec::MAX_FONT_SLOTS {
            return ERR_BAD_ARGUMENT;
        }
        state.surface.with_ui(|ui| {
            let Some(atlas) = ui.font_atlas(slot as u8) else {
                return 1;
            };
            unsafe {
                *out = PocketHitagiFont {
                    coverage: atlas.bitmap.as_ptr(),
                    byte_count: atlas.bitmap.len(),
                    revision: ui.font_atlas_revision(slot as u8),
                    cell_width: atlas.cell_w,
                    cell_height: atlas.cell_h,
                    coverage_width: atlas.coverage_width(),
                    coverage_height: atlas.coverage_height(),
                    glyph_count: atlas.glyph_count as u32,
                };
            }
            OK
        })
    })
}

// C++ decoder follows this pinned upstream contract. Fail the package build if
// an upstream update changes the opcodes or formats instead of drawing garbage.
const _: () = {
    assert!(
        spec::draw_op::RECT == 1 && spec::draw_op::GRAD_RECT == 2 && spec::draw_op::GLYPH_RUN == 3
    );
    assert!(
        spec::draw_op::TEX_QUAD == 4
            && spec::draw_op::SCISSOR == 5
            && spec::draw_op::SCISSOR_POP == 6
    );
    assert!(
        spec::draw_op::TRI == 7
            && spec::draw_op::TEX_TRI == 8
            && spec::draw_op::TEXT_RUN == 9
            && spec::draw_op::SURFACE_QUAD == 10
    );
    assert!(
        spec::psm::PSM_5650 == 0
            && spec::psm::PSM_4444 == 2
            && spec::psm::PSM_8888 == 3
            && spec::psm::PSM_T8 == 5
    );
};
