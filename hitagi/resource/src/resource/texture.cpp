#include <hitagi/resource/texture.hpp>

namespace hitagi::resource {
Texture::Texture(std::filesystem::path path, allocator_type alloc)
    : SceneObject(alloc), m_ImagePath(std::move(path)) {}

Texture::Texture(const std::shared_ptr<Image>& image, allocator_type alloc)
    : SceneObject(alloc), m_Image(image) {}

Texture::Texture(uint32_t coord_index, const std::shared_ptr<Image>& image, allocator_type alloc)
    : SceneObject(alloc), m_TexCoordIndex(coord_index), m_Image(image) {}

inline std::shared_ptr<Image> Texture::GetTextureImage() {
    return m_Image.lock();
}

void Texture::LoadImage(std::function<std::shared_ptr<Image>(std::filesystem::path)>&& loader) {
    m_Image = loader(m_ImagePath);
}

}  // namespace hitagi::resource