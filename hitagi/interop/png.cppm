module;
#include <png.h>

namespace hitagi::interop {
// Only obtain the jump buffer here; setjmp must execute in the caller's frame.
inline auto           png_jump_buffer(png_structp png) -> jmp_buf& { return png_jmpbuf(png); }
inline constexpr auto png_color_type_gray          = PNG_COLOR_TYPE_GRAY;
inline constexpr auto png_color_type_gray_alpha    = PNG_COLOR_TYPE_GRAY_ALPHA;
inline constexpr auto png_color_type_rgb           = PNG_COLOR_TYPE_RGB;
inline constexpr auto png_color_type_rgba          = PNG_COLOR_TYPE_RGBA;
inline constexpr auto png_compression_type_default = PNG_COMPRESSION_TYPE_DEFAULT;
inline constexpr auto png_filter_type_default      = PNG_FILTER_TYPE_DEFAULT;
inline constexpr auto png_interlace_none           = PNG_INTERLACE_NONE;
inline constexpr auto png_libpng_ver_string        = PNG_LIBPNG_VER_STRING;
inline constexpr auto png_transform_expand         = PNG_TRANSFORM_EXPAND;
inline constexpr auto png_transform_packing        = PNG_TRANSFORM_PACKING;
inline constexpr auto png_transform_strip_16       = PNG_TRANSFORM_STRIP_16;
}  // namespace hitagi::interop
#undef PNG_COLOR_TYPE_GRAY
#undef PNG_COLOR_TYPE_GRAY_ALPHA
#undef PNG_COLOR_TYPE_RGB
#undef PNG_COLOR_TYPE_RGBA
#undef PNG_COMPRESSION_TYPE_DEFAULT
#undef PNG_FILTER_TYPE_DEFAULT
#undef PNG_INTERLACE_NONE
#undef PNG_LIBPNG_VER_STRING
#undef PNG_TRANSFORM_EXPAND
#undef PNG_TRANSFORM_PACKING
#undef PNG_TRANSFORM_STRIP_16

export module interop.png;

export {
    using ::png_byte;
    using ::png_bytep;
    using ::png_bytepp;
    using ::png_const_bytep;
    using ::png_create_info_struct;
    using ::png_create_read_struct;
    using ::png_create_write_struct;
    using ::png_destroy_read_struct;
    using ::png_destroy_write_struct;
    using ::png_error;
    using ::png_get_color_type;
    using ::png_get_image_height;
    using ::png_get_image_width;
    using ::png_get_io_ptr;
    using ::png_get_rows;
    using ::png_infop;
    using ::png_read_png;
    using ::png_set_IHDR;
    using ::png_set_read_fn;
    using ::png_set_write_fn;
    using ::png_sig_cmp;
    using ::png_size_t;
    using ::png_structp;
    using ::png_write_end;
    using ::png_write_info;
    using ::png_write_row;
    inline constexpr auto PNG_COLOR_TYPE_GRAY          = hitagi::interop::png_color_type_gray;
    inline constexpr auto PNG_COLOR_TYPE_GRAY_ALPHA    = hitagi::interop::png_color_type_gray_alpha;
    inline constexpr auto PNG_COLOR_TYPE_RGB           = hitagi::interop::png_color_type_rgb;
    inline constexpr auto PNG_COLOR_TYPE_RGBA          = hitagi::interop::png_color_type_rgba;
    inline constexpr auto PNG_COMPRESSION_TYPE_DEFAULT = hitagi::interop::png_compression_type_default;
    inline constexpr auto PNG_FILTER_TYPE_DEFAULT      = hitagi::interop::png_filter_type_default;
    inline constexpr auto PNG_INTERLACE_NONE           = hitagi::interop::png_interlace_none;
    inline constexpr auto PNG_LIBPNG_VER_STRING        = hitagi::interop::png_libpng_ver_string;
    inline constexpr auto PNG_TRANSFORM_EXPAND         = hitagi::interop::png_transform_expand;
    inline constexpr auto PNG_TRANSFORM_PACKING        = hitagi::interop::png_transform_packing;
    inline constexpr auto PNG_TRANSFORM_STRIP_16       = hitagi::interop::png_transform_strip_16;
}
export namespace hitagi::interop {
using ::hitagi::interop::png_jump_buffer;
}
