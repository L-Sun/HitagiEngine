#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/image.hpp>

#include <filesystem>

namespace hitagi::resource {
class Texture : public SceneObject {
public:
    Texture(std::filesystem::path path, allocator_type alloc = {});
    Texture(const std::shared_ptr<Image>& image, allocator_type alloc = {});
    Texture(uint32_t coord_index, const std::shared_ptr<Image>& image, allocator_type alloc = {});

    std::shared_ptr<Image> GetTextureImage();

    void LoadImage(std::function<std::shared_ptr<Image>(std::filesystem::path)>&& loader);

protected:
    uint32_t              m_TexCoordIndex = 0;
    std::filesystem::path m_ImagePath;
    // Use weak ptr, so we can unload image if the texture no used;
    std::weak_ptr<Image> m_Image;
};

}  // namespace hitagi::resource