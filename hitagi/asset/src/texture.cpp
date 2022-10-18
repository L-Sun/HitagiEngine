#include <hitagi/asset/texture.hpp>
#include <hitagi/asset/parser/image_parser.hpp>
#include <hitagi/core/file_io_manager.hpp>

namespace hitagi::asset {
Texture::Texture(std::uint32_t width, std::uint32_t height, gfx::Format format, core::Buffer data, std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      m_Width(width),
      m_Height(height),
      m_Format(format),
      m_CpuData(std::move(data)) {}

Texture::Texture(std::filesystem::path path, std::string_view name, xg::Guid guid)
    : Resource(name.empty() ? path.string() : name, guid),
      m_Path(std::move(path)) {}

Texture::Texture(const Texture& other)
    : Resource(other),
      m_Width(other.m_Width),
      m_Height(other.m_Height),
      m_Format(other.m_Format),
      m_Path(other.m_Path),
      m_CpuData(other.m_CpuData) {
    m_Path.replace_filename(m_Path.filename().concat("_copy"));
}

Texture& Texture::operator=(const Texture& rhs) {
    if (this != &rhs) {
        Resource::operator=(rhs);
        m_Width   = rhs.m_Width;
        m_Height  = rhs.m_Height;
        m_Format  = rhs.m_Format;
        m_Path    = rhs.m_Path;
        m_CpuData = rhs.m_CpuData;

        if (m_GpuData) {
            InitGpuData(m_GpuData->device);
        }
        m_Path.replace_filename(m_Path.filename().concat("_copy"));
    }
    return *this;
}

bool Texture::SetPath(const std::filesystem::path& path) {
    if (get_image_format(path.string()) == ImageFormat::UNKOWN) {
        return false;
    }
    m_Path = path;
    return true;
}

bool Texture::Load(const std::shared_ptr<ImageParser>& parser) {
    if (parser == nullptr || !std::filesystem::is_regular_file(m_Path))
        return false;

    if (file_io_manager) {
        auto image = parser->Parse(file_io_manager->SyncOpenAndReadBinary(m_Path));
        if (image == nullptr) return false;
        m_Width   = image->m_Width;
        m_Height  = image->m_Height;
        m_Format  = image->m_Format;
        m_CpuData = std::move(image->m_CpuData);
        if (m_GpuData) {
            InitGpuData(m_GpuData->device);
        }
        return true;
    }

    return false;
}

void Texture::Unload() {
    if (!m_Path.empty() && std::filesystem::is_regular_file(m_Path)) {
        m_CpuData = {};
    }
    m_GpuData = nullptr;
}

bool Texture::InitGpuData(gfx::Device& device) {
    m_GpuData = device.CreateTexture({
        .name   = m_Name,
        .width  = m_Width,
        .height = m_Height,
        .format = m_Format,
    });
    return m_GpuData != nullptr;
}

}  // namespace hitagi::asset