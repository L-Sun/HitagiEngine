#pragma once
#include "AssetManager.hpp"

namespace Hitagi::Asset {
class Texture {
protected:
    uint32_t               m_TexCoordIndex = 0;
    std::filesystem::path  m_TexturePath;
    std::shared_ptr<Image> m_Image = nullptr;

public:
    Texture(const std::filesystem::path& path) : m_TexturePath(path) {}
    Texture(std::string name, uint32_t coord_index, std::shared_ptr<Image> image)
        : m_TexCoordIndex(coord_index), m_Image(std::move(image)) {}
    Texture(Texture&)  = default;
    Texture(Texture&&) = default;

    inline void                   LoadTexture() { m_Image = g_AssetManager->ImportImage(m_TexturePath); }
    inline std::shared_ptr<Image> GetTextureImage() const noexcept { return m_Image; }
    friend std::ostream&          operator<<(std::ostream& out, const Texture& obj);
};

}  // namespace Hitagi::Asset