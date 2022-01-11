#include "Texture.hpp"
#include "AssetManager.hpp"

namespace Hitagi::Asset {
void Texture::LoadTexture() { m_Image = g_AssetManager->ImportImage(m_TexturePath); }

std::ostream& operator<<(std::ostream& out, const Texture& obj) {
    out << "Coord Index: " << obj.m_TexCoordIndex << std::endl;
    out << "Path:        " << obj.m_TexturePath << std::endl;
    if (!obj.m_Image) out << "Image:\n"
                          << obj.m_Image;
    return out;
}
}  // namespace Hitagi::Asset