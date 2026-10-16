#pragma once
#include "pocket_apple.h"

#ifdef __cplusplus
namespace pocket {
// PocketJS 0.14.0, commit 55e39a06868c9776f0320909ba37b187f0606d0f:
// contracts/spec/spec.ts defines DRAW_OP, PSM, ENUMS.GradDir and wire layouts.
// The upstream C headers do not expose these enums. Review this adapter when
// upgrading the pinned package, including the command lengths below.
enum class DrawOp : uint32_t {
    Rect            = 1,
    GradientRect    = 2,
    GlyphRun        = 3,
    TextureQuad     = 4,
    Scissor         = 5,
    ScissorPop      = 6,
    Triangle        = 7,
    TextureTriangle = 8,
    TextRun         = 9,
    SurfaceQuad     = 10,
};

enum class PixelFormat : uint32_t {
    Bgr565   = 0,  // PSM_5650: B5:G6:R5, red in the least-significant bits.
    Abgr4444 = 2,  // PSM_4444.
    Abgr8888 = 3,  // PSM_8888: little-endian bytes are R, G, B, A.
    Indexed8 = 5,  // PSM_T8: byte indices into a 256-entry ABGR8888 palette.
};

enum class GradientDirection : uint32_t {
    ToTop    = 0,
    ToBottom = 1,
    ToLeft   = 2,
    ToRight  = 3,
};

// Includes the opcode word; GlyphRun and TextRun also have variable payloads.
constexpr auto FixedWordCount(DrawOp op) -> uint32_t {
    switch (op) {
        case DrawOp::Rect:
            return 4;
        case DrawOp::GradientRect:
            return 6;
        case DrawOp::GlyphRun:
            return 3;
        case DrawOp::TextureQuad:
            return 9;
        case DrawOp::Scissor:
            return 3;
        case DrawOp::ScissorPop:
            return 1;
        case DrawOp::Triangle:
            return 7;
        case DrawOp::TextureTriangle:
            return 12;
        case DrawOp::TextRun:
            return 8;
        case DrawOp::SurfaceQuad:
            return 9;
    }
    return 0;  // Unknown wire opcode.
}

inline constexpr uint32_t rgba_bytes_per_pixel = 4;
inline constexpr uint32_t palette_entries      = 256;
inline constexpr uint32_t glyph_words          = 2;
inline constexpr uint32_t bytes_per_word       = sizeof(uint32_t);
// Package extension contract (pocket.h / pocket_gpu.rs).
inline constexpr int      resource_present       = 0;
inline constexpr int      resource_vacant        = 1;
inline constexpr uint32_t max_viewport_dimension = 8192;

constexpr auto BytesPerPixel(PixelFormat format) -> size_t {
    switch (format) {
        case PixelFormat::Bgr565:
        case PixelFormat::Abgr4444:
            return 2;
        case PixelFormat::Abgr8888:
            return rgba_bytes_per_pixel;
        case PixelFormat::Indexed8:
            return 1;
    }
    return 0;
}

// Return zero for unknown or truncated commands. Read variable lengths only
// after validating the fixed header, and compare before multiplying sizes.
constexpr auto CommandWordCount(const uint32_t* words, size_t available) -> size_t {
    if (!available) return 0;
    const auto op    = static_cast<DrawOp>(words[0]);
    const auto fixed = FixedWordCount(op);
    if (!fixed || available < fixed) return 0;
    size_t payload = 0;
    if (op == DrawOp::GlyphRun) {
        const auto count = words[1] >> 16;
        if (count > (available - fixed) / glyph_words) return 0;
        payload = size_t(count) * glyph_words;
    } else if (op == DrawOp::TextRun) {
        const auto bytes = words[7];
        payload          = bytes / bytes_per_word + (bytes % bytes_per_word != 0);
        if (payload > available - fixed) return 0;
    }
    return fixed + payload;
}
}  // namespace pocket
extern "C" {
#endif
// Package extension, not an upstream ABI. Preserves the mounted guest on resize.
// Logical dimensions are 1..8192. Software surfaces cap allocation at 256 MiB.
int32_t pocket_resize(PocketApple* handle, uint32_t width, uint32_t height);

// Creates the same guest/UiSurface without a CPU framebuffer. Use draw(), never
// pocket_apple_render(), with this handle. Other pocket_apple lifecycle APIs apply.
PocketApple* pocket_create_gpu(uint32_t density, uint32_t width, uint32_t height);

// All views borrow core memory until the next mutating call or destroy. Consume
// them synchronously on the runtime thread; never retain them in a render graph.
typedef struct {
    const uint32_t* words;
    size_t word_count;
    uint32_t texture_slots;
    uint32_t font_slots;
} PocketDrawList;
typedef struct {
    const uint8_t* pixels;
    size_t byte_count;
    const uint8_t* palette;
    size_t palette_bytes;
    uint64_t revision;
    int32_t handle;
    uint32_t width, height;
#ifdef __cplusplus
    pocket::PixelFormat format;
#else
    uint32_t format;
#endif
    uint32_t linear;
} PocketTexture;
typedef struct {
    const uint8_t* coverage;
    size_t byte_count;
    uint64_t revision;
    uint32_t cell_width, cell_height;
    uint32_t coverage_width, coverage_height, glyph_count;
} PocketFont;
int32_t pocket_draw(PocketApple* handle, PocketDrawList* out);
// 0 = present, 1 = vacant slot, negative = error.
int32_t pocket_texture(PocketApple* handle, uint32_t slot, PocketTexture* out);
int32_t pocket_font(PocketApple* handle, uint32_t slot, PocketFont* out);
#ifdef __cplusplus
}
#endif
