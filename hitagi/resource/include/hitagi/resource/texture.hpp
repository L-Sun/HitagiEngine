#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/enums.hpp>
#include <hitagi/resource/image.hpp>

#include <filesystem>

namespace hitagi::resource {

class Texture : public ResourceObject {
public:
    Texture(std::filesystem::path path);
    Texture(std::shared_ptr<Image> image);
    Texture(std::uint32_t coord_index, std::shared_ptr<Image> image);

    std::shared_ptr<Image> GetTextureImage() const noexcept;
    std::filesystem::path  GetTexturePath() const noexcept;

    void LoadImage(std::function<std::shared_ptr<Image>(std::filesystem::path)>&& loader);
    void UnloadImage();

protected:
    uint32_t              m_TexCoordIndex = 0;
    std::filesystem::path m_ImagePath;
    // Use weak ptr, so we can unload image if the texture no used;
    std::shared_ptr<Image> m_Image;
};

}  // namespace hitagi::resource