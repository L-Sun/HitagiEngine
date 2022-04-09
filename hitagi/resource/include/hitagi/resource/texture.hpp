#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/image.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace hitagi::resource {
class Texture : public SceneObject {
public:
    Texture(std::shared_ptr<Image> image) : m_Image(std::move(image)) {}
    Texture(std::string_view name, uint32_t coord_index, std::shared_ptr<Image> image)
        : SceneObject(name), m_TexCoordIndex(coord_index), m_Image(std::move(image)) {}
    Texture(Texture&)  = default;
    Texture(Texture&&) = default;

    void                          LoadTexture();
    inline std::shared_ptr<Image> GetTextureImage() const noexcept { return m_Image; }
    friend std::ostream&          operator<<(std::ostream& out, const Texture& obj);

protected:
    uint32_t               m_TexCoordIndex = 0;
    std::shared_ptr<Image> m_Image         = nullptr;
};

}  // namespace hitagi::resource