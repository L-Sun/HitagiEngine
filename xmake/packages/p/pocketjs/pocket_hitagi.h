#pragma once
#include "pocket_apple.h"

#ifdef __cplusplus
extern "C" {
#endif
// Hitagi extension, not an upstream ABI. Preserves the mounted guest on resize.
// Logical dimensions are 1..8192. Software surfaces cap allocation at 256 MiB.
int32_t pocket_hitagi_resize(PocketApple* handle, uint32_t width, uint32_t height);

// Creates the same guest/UiSurface without a CPU framebuffer. Use draw(), never
// pocket_apple_render(), with this handle. Other pocket_apple lifecycle APIs apply.
PocketApple* pocket_hitagi_create_gpu(uint32_t density, uint32_t width, uint32_t height);

// All views borrow core memory until the next mutating call or destroy. Consume
// them synchronously on the runtime thread; never retain them in a render graph.
typedef struct {
    const uint32_t* words;
    size_t word_count;
    uint32_t texture_slots;
    uint32_t font_slots;
} PocketHitagiDrawList;
typedef struct {
    const uint8_t* pixels;
    size_t byte_count;
    const uint8_t* palette;
    size_t palette_bytes;
    uint64_t revision;
    int32_t handle;
    uint32_t width, height, format, linear;
} PocketHitagiTexture;
typedef struct {
    const uint8_t* coverage;
    size_t byte_count;
    uint64_t revision;
    uint32_t cell_width, cell_height;
    uint32_t coverage_width, coverage_height, glyph_count;
} PocketHitagiFont;
int32_t pocket_hitagi_draw(PocketApple* handle, PocketHitagiDrawList* out);
// 0 = present, 1 = vacant slot, negative = error.
int32_t pocket_hitagi_texture(PocketApple* handle, uint32_t slot, PocketHitagiTexture* out);
int32_t pocket_hitagi_font(PocketApple* handle, uint32_t slot, PocketHitagiFont* out);
#ifdef __cplusplus
}
#endif
