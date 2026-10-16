module;
#include "pocket_apple.h"
#include "pocket_hitagi.h"
#include "pocket_spec.h"

export module interop.pocketjs;

export namespace hitagi::interop {
inline constexpr auto pocket_button_up    = POCKET_BTN_UP;
inline constexpr auto pocket_button_right = POCKET_BTN_RIGHT;
inline constexpr auto pocket_button_down  = POCKET_BTN_DOWN;
inline constexpr auto pocket_button_left  = POCKET_BTN_LEFT;
// PocketJS's portable focus controller activates with CIRCLE.
inline constexpr auto pocket_button_confirm = POCKET_BTN_CIRCLE;
inline constexpr auto pocket_button_cancel  = POCKET_BTN_CROSS;
}  // namespace hitagi::interop

// Upstream names are retained to preserve the C ABI and error semantics.
export {
    using ::pocket_apple_abi_version;
    using ::pocket_apple_create;
    using ::pocket_apple_destroy;
    using ::pocket_apple_eval_bundle;
    using ::pocket_apple_frame;
    using ::pocket_apple_hit_test_bounds;
    using ::pocket_apple_last_error;
    using ::pocket_apple_load_pak;
    using ::pocket_apple_post_event;
    using ::pocket_apple_render;
    using ::pocket_apple_set_effect_callback;
    using ::pocket_apple_set_identity;
    using ::pocket_apple_set_svc_allowlist;
    using ::pocket_hitagi_resize;
    using ::pocket_hitagi_create_gpu;
    using ::pocket_hitagi_draw;
    using ::pocket_hitagi_texture;
    using ::pocket_hitagi_font;
    using ::PocketHitagiDrawList;
    using ::PocketHitagiTexture;
    using ::PocketHitagiFont;
    using ::PocketApple;
    using ::PocketAppleFrame;
}
