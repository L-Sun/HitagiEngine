#include <hitagi/asset/texture.hpp>
#include <hitagi/asset/parser/image_parser.hpp>
#include <hitagi/core/file_io_manager.hpp>

namespace hitagi::asset {
Texture::Texture(std::uint32_t width, std::uint32_t height, gfx::Format format, core::Buffer data, std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      m_Width(width),
      m_Height(height),
      m_Format(format),
      m_CPUData(std::move(data)) {}

Texture::Texture(std::filesystem::path path, std::string_view name, xg::Guid guid)
    : Resource(name.empty() ? path.string() : name, guid),
      m_Path(std::move(path)) {}

Texture::Texture(const Texture& other)
    : Resource(other),
      m_Width(other.m_Width),
      m_Height(other.m_Height),
      m_Format(other.m_Format),
      m_Path(other.m_Path),
      m_CPUData(other.m_CPUData) {
    m_Path.replace_filename(m_Path.filename().concat("_copy"));
}

Texture& Texture::operator=(const Texture& rhs) {
    if (this != &rhs) {
        Resource::operator=(rhs);
        m_Width   = rhs.m_Width;
        m_Height  = rhs.m_Height;
        m_Format  = rhs.m_Format;
        m_Path    = rhs.m_Path;
        m_CPUData = rhs.m_CPUData;
        m_Dirty   = true;

        if (m_GPUData) {
            InitGPUData(m_GPUData->GetDevice());
        }
        m_Path.replace_filename(m_Path.filename().concat("_copy"));
    }
    return *this;
}

std::shared_ptr<Texture> Texture::m_DefaultTexture = nullptr;

auto Texture::DefaultTexture() -> std::shared_ptr<Texture> {
    constexpr math::Vector<std::uint8_t, 4> color(216, 115, 255, 255);

    // TODO thread safe
    if (m_DefaultTexture == nullptr) {
        m_DefaultTexture = std::make_shared<Texture>(
            1, 1, gfx::Format::R8G8B8A8_UNORM,
            core::Buffer(4, reinterpret_cast<const std::byte*>(&color)),
            "DefaultTexture");
    }
    return m_DefaultTexture;
}

void Texture::DestoryDefaultTexture() {
    m_DefaultTexture = nullptr;
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
        m_CPUData = std::move(image->m_CPUData);
        m_Dirty   = true;
        if (m_GPUData) {
            InitGPUData(m_GPUData->GetDevice());
        }
        return true;
    }

    return false;
}

void Texture::Unload() {
    if (!m_Path.empty() && std::filesystem::is_regular_file(m_Path)) {
        m_CPUData = {};
    }
    m_GPUData = nullptr;
}

void Texture::InitGPUData(gfx::Device& device) {
    if (!m_Dirty) return;
    m_GPUData = device.CreateTexture({
        .name   = m_Name,
        .width  = m_Width,
        .height = m_Height,
        .format = m_Format,
    });
    m_Dirty   = false;
}

}  // namespace hitagi::asset