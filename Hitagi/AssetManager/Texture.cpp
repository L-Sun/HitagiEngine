#include "Texture.hpp"

namespace Hitagi::Asset {
std::ostream& operator<<(std::ostream& out, const Texture& obj) {
    out << "Coord Index: " << obj.m_TexCoordIndex << std::endl;
    out << "Path:        " << obj.m_TexturePath << std::endl;
    if (!obj.m_Image) out << "Image:\n"
                          << obj.m_Image;
    return out;
}
}  // namespace Hitagi::Asset