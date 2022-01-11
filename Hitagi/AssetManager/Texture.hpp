#pragma once
#include "Image.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace Hitagi::Asset {
class Texture {
public:
    Texture(const std::filesystem::path& path) : m_TexturePath(path) {}
    Texture(std::string name, uint32_t coord_index, std::shared_ptr<Image> image)
        : m_TexCoordIndex(coord_index), m_Image(std::move(image)) {}
    Texture(Texture&)  = default;
    Texture(Texture&&) = default;

    void                          LoadTexture();
    inline std::shared_ptr<Image> GetTextureImage() const noexcept { return m_Image; }
    friend std::ostream&          operator<<(std::ostream& out, const Texture& obj);

protected:
    uint32_t               m_TexCoordIndex = 0;
    std::filesystem::path  m_TexturePath;
    std::shared_ptr<Image> m_Image = nullptr;
};

}  // namespace Hitagi::Asset