#include <hitagi/resource/texture.hpp>
#include <hitagi/resource/asset_manager.hpp>

namespace hitagi::asset {
void Texture::LoadTexture() { m_Image = g_AssetManager->ImportImage(m_TexturePath); }

std::ostream& operator<<(std::ostream& out, const Texture& obj) {
    out << "Coord Index: " << obj.m_TexCoordIndex << std::endl;
    out << "Path:        " << obj.m_TexturePath << std::endl;
    if (!obj.m_Image) out << "Image:\n"
                          << obj.m_Image;
    return out;
}
}  // namespace hitagi::asset