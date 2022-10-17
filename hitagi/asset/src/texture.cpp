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