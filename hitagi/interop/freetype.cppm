module;
#include <ft2build.h>
#include FT_FREETYPE_H

namespace hitagi::interop {
inline bool           has_kerning(FT_Face face) { return FT_HAS_KERNING(face); }
inline constexpr auto ft_load_render        = FT_LOAD_RENDER;
inline constexpr auto ft_load_target_normal = FT_LOAD_TARGET_NORMAL;
}  // namespace hitagi::interop
#undef FT_LOAD_RENDER
#undef FT_LOAD_TARGET_NORMAL

export module interop.freetype;

export {
    using ::FT_Bitmap;
    using ::FT_Done_Face;
    using ::FT_Done_FreeType;
    using ::FT_Face;
    using ::FT_Get_Char_Index;
    using ::FT_Get_Kerning;
    using ::FT_Init_FreeType;
    using ::FT_KERNING_DEFAULT;
    using ::FT_Library;
    using ::FT_Load_Glyph;
    using ::FT_New_Face;
    using ::FT_PIXEL_MODE_GRAY;
    using ::FT_Set_Pixel_Sizes;
    using ::FT_UInt;
    using ::FT_ULong;
    using ::FT_Vector;
    inline constexpr auto FT_LOAD_RENDER        = hitagi::interop::ft_load_render;
    inline constexpr auto FT_LOAD_TARGET_NORMAL = hitagi::interop::ft_load_target_normal;
}
export namespace hitagi::interop {
using ::hitagi::interop::has_kerning;
}
