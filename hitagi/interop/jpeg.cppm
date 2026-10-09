module;
#include <cstdio>
#include <jpeglib.h>

namespace hitagi::interop {
inline void create_jpeg_decompress(j_decompress_ptr info) { jpeg_create_decompress(info); }
}  // namespace hitagi::interop

export module interop.jpeg;

export {
    using ::j_common_ptr;
    using ::JCS_EXT_RGBA;
    using ::jpeg_decompress_struct;
    using ::jpeg_destroy_decompress;
    using ::jpeg_error_mgr;
    using ::jpeg_finish_decompress;
    using ::jpeg_mem_src;
    using ::jpeg_read_header;
    using ::jpeg_read_scanlines;
    using ::jpeg_start_decompress;
    using ::jpeg_std_error;
    using ::JSAMPLE;
}
export namespace hitagi::interop {
using ::hitagi::interop::create_jpeg_decompress;
}
