module;
#include "pocket_apple.h"
#include "pocket.h"
#include "pocket_spec.h"

export module interop.pocketjs;

export namespace hitagi::interop::pocketjs {
enum class Button : uint32_t {
    None   = 0,
    Up     = POCKET_BTN_UP,
    Right  = POCKET_BTN_RIGHT,
    Down   = POCKET_BTN_DOWN,
    Left   = POCKET_BTN_LEFT,
    Circle = POCKET_BTN_CIRCLE,
    Cross  = POCKET_BTN_CROSS,
};

using ::pocket::bytes_per_word;
using ::pocket::BytesPerPixel;
using ::pocket::CommandWordCount;
using ::pocket::DrawOp;
using ::pocket::FixedWordCount;
using ::pocket::glyph_words;
using ::pocket::GradientDirection;
using ::pocket::max_viewport_dimension;
using ::pocket::palette_entries;
using ::pocket::PixelFormat;
using ::pocket::resource_present;
using ::pocket::resource_vacant;
using ::pocket::rgba_bytes_per_pixel;
}  // namespace hitagi::interop::pocketjs

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
    using ::pocket_create_gpu;
    using ::pocket_draw;
    using ::pocket_font;
    using ::pocket_resize;
    using ::pocket_texture;
    using ::PocketApple;
    using ::PocketAppleFrame;
    using ::PocketDrawList;
    using ::PocketFont;
    using ::PocketTexture;
}
