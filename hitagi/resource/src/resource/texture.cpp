#include <hitagi/resource/texture.hpp>

namespace hitagi::resource {
Texture::Texture(std::filesystem::path path, allocator_type alloc)
    : SceneObject(alloc), m_ImagePath(std::move(path)) {}

Texture::Texture(std::shared_ptr<Image> image, allocator_type alloc)
    : SceneObject(alloc), m_Image(std::move(image)) {}

Texture::Texture(uint32_t coord_index, std::shared_ptr<Image> image, allocator_type alloc)
    : SceneObject(alloc), m_TexCoordIndex(coord_index), m_Image(std::move(image)) {}

inline std::shared_ptr<Image> Texture::GetTextureImage() {
    return m_Image;
}

void Texture::LoadImage(std::function<std::shared_ptr<Image>(std::filesystem::path)>&& loader) {
    m_Image = loader(m_ImagePath);
}

void Texture::UnloadImage() {
    m_Image = nullptr;
}

}  // namespace hitagi::resource