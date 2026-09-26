module;

export module asset:texture;
import std;
import core;
import gfx;
import :resource;
import :image_codec;

export namespace hitagi::asset {

// Produces a texture's CPU pixels on demand. Called from a worker thread when the
// texture was given a decode submitter, so it must not touch gfx. Throws on failure.
using ImageLoader = std::function<ImageData()>;

// Loader that reads `path` through `file_io` and decodes it with the codec picked
// by the file extension. `file_io` must outlive every texture holding the loader.
auto MakeFileImageLoader(core::FileIOManager& file_io, std::filesystem::path path) -> ImageLoader;

class Texture : public Resource {
public:
    Texture(std::uint32_t    width,
            std::uint32_t    height,
            gfx::Format      format,
            core::Buffer     data = {},
            std::string_view name = "");
    Texture(ImageData image_data, std::string_view name = "");
    // Lazily loaded texture: `path` is its identity, `loader` produces the pixels
    // on first Load(). An empty `decode_submitter` means Load() decodes synchronously.
    Texture(std::filesystem::path path,
            ImageLoader           loader,
            std::string_view      name             = "",
            core::JobSubmitter    decode_submitter = {});

    Texture(Texture&&) noexcept            = default;
    Texture& operator=(Texture&&) noexcept = default;

    static auto DefaultTexture() -> std::shared_ptr<Texture>;
    static void DestroyDefaultTexture();

    inline auto Empty() const noexcept { return m_ImageData.data.Empty() && m_Path.empty(); }
    inline auto Width() const noexcept { return m_ImageData.width; }
    inline auto Height() const noexcept { return m_ImageData.height; }
    inline auto Format() const noexcept { return m_ImageData.format; }
    inline auto GetData() const noexcept { return m_ImageData.data.Span<const std::byte>(); }
    inline const auto& GetPath() const noexcept { return m_Path; }
    inline auto GetGPUData() const noexcept { return m_GPUData; }
    inline auto GetGPUView() const noexcept { return m_GPUView; }

    bool SetPath(const std::filesystem::path& path);

    void Load(const ResourceLoadContext& context) final;
    void Unload() final;

private:
    // Worker-thread half of the async path: runs the loader (-> Staged/Failed).
    // Never touches gfx. Safe to call off the render thread.
    void DecodeCPU() noexcept;
    void Upload(gfx::Device& device);

    ImageData                    m_ImageData;
    std::filesystem::path        m_Path;
    ImageLoader                  m_Loader;
    core::JobSubmitter           m_DecodeSubmitter;
    std::shared_ptr<gfx::Texture>     m_GPUData = nullptr;
    std::shared_ptr<gfx::TextureView> m_GPUView = nullptr;

    static std::shared_ptr<Texture> m_DefaultTexture;
};
}  // namespace hitagi::asset

namespace hitagi::asset {

auto ImageCodec::Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture> {
    auto image_data = DecodeImageData(buffer);
    if (!image_data) return nullptr;
    return std::make_shared<Texture>(std::move(image_data));
}

auto ImageCodec::Encode(const Texture& texture) -> core::Buffer {
    const auto data = texture.GetData();
    return EncodeImageData(ImageData{
        .width  = texture.Width(),
        .height = texture.Height(),
        .format = texture.Format(),
        .data   = core::Buffer(data.size(), data.data()),
    });
}

auto PngEncoder::Encode(const Texture& texture) -> core::Buffer {
    return PngDecoder{}.Encode(texture);
}

Texture::Texture(std::uint32_t width, std::uint32_t height, gfx::Format format, core::Buffer data, std::string_view name)
    : Resource(Type::Texture, name),
      m_ImageData{
          .width  = width,
          .height = height,
          .format = format,
          .data   = std::move(data),
      } {}

Texture::Texture(ImageData image_data, std::string_view name)
    : Resource(Type::Texture, name),
      m_ImageData(std::move(image_data)) {}

auto MakeFileImageLoader(core::FileIOManager& file_io, std::filesystem::path path) -> ImageLoader {
    return [&file_io, path = std::move(path)]() -> ImageData {
        const auto codec = create_image_codec_for(path.extension());
        if (codec == nullptr) {
            throw std::runtime_error("Failed to create image codec for " + path.string());
        }
        return codec->DecodeImageData(file_io.SyncOpenAndReadBinary(path));
    };
}

Texture::Texture(std::filesystem::path path, ImageLoader loader, std::string_view name, core::JobSubmitter decode_submitter)
    : Resource(Type::Texture, name.empty() ? path.string() : name),
      m_Path(std::move(path)),
      m_Loader(std::move(loader)),
      m_DecodeSubmitter(std::move(decode_submitter)) {}

std::shared_ptr<Texture> Texture::m_DefaultTexture = nullptr;

auto Texture::DefaultTexture() -> std::shared_ptr<Texture> {
    if (m_DefaultTexture == nullptr) {
        constexpr std::array pixels{
            static_cast<std::byte>(216),
            static_cast<std::byte>(115),
            static_cast<std::byte>(255),
            static_cast<std::byte>(255),
            static_cast<std::byte>(216),
            static_cast<std::byte>(115),
            static_cast<std::byte>(255),
            static_cast<std::byte>(255),
            static_cast<std::byte>(216),
            static_cast<std::byte>(115),
            static_cast<std::byte>(255),
            static_cast<std::byte>(255),
            static_cast<std::byte>(216),
            static_cast<std::byte>(115),
            static_cast<std::byte>(255),
            static_cast<std::byte>(255),
        };
        m_DefaultTexture = std::make_shared<Texture>(
            2,
            2,
            gfx::Format::R8G8B8A8_UNORM,
            core::Buffer(pixels.size(), pixels.data()),
            "DefaultTexture");
    }
    return m_DefaultTexture;
}

void Texture::DestroyDefaultTexture() {
    m_DefaultTexture = nullptr;
}

bool Texture::SetPath(const std::filesystem::path& path) {
    if (create_image_codec_for(path.extension()) == nullptr) return false;
    m_Path = path;
    return true;
}

void Texture::DecodeCPU() noexcept {
    try {
        if (m_ImageData.data.Empty() && m_Loader) {
            m_ImageData = m_Loader();
        }
        // SetLoadState's release order publishes m_ImageData to the render thread.
        SetLoadState(ResourceLoadState::Staged);
    } catch (...) {
        SetLoadState(ResourceLoadState::Failed);
    }
}

void Texture::Upload(gfx::Device& device) {
    m_GPUData = device.CreateTexture(
        {
            .name   = m_Name,
            .width  = m_ImageData.width,
            .height = m_ImageData.height,
            .format = m_ImageData.format,
            .usages = gfx::TextureUsageFlags::SRV | gfx::TextureUsageFlags::CopyDst,
        },
        m_ImageData.data.Span<const std::byte>());
    m_GPUView = device.CreateTextureView({
        .name    = std::pmr::string(std::format("{}-srv", m_Name)),
        .texture = m_GPUData,
        .type    = gfx::TextureViewType::ShaderRead,
    });
    SetLoadState(ResourceLoadState::Loaded);
}

void Texture::Load(const ResourceLoadContext& context) {
    switch (GetLoadState()) {
        case ResourceLoadState::Loaded:
        case ResourceLoadState::Failed:
        case ResourceLoadState::Loading:  // in flight; caller keeps using the placeholder
            return;
        case ResourceLoadState::Staged:
            Upload(context.device);
            return;
        case ResourceLoadState::Unloaded:
            break;
    }

    if (m_ImageData.data.Empty() && m_Loader) {
        // Lazily loaded: decode asynchronously when a submitter was injected and this
        // texture is shared-owned (the job needs to keep it alive).
        const auto self = std::static_pointer_cast<Texture>(weak_from_this().lock());
        if (self && m_DecodeSubmitter) {
            SetLoadState(ResourceLoadState::Loading);
            m_DecodeSubmitter([self] { self->DecodeCPU(); });
            return;
        }
        // Sync path (no submitter / unowned texture): preserve the old throwing behavior.
        DecodeCPU();
        if (GetLoadState() == ResourceLoadState::Failed) {
            throw std::runtime_error("Failed to decode texture " + m_Path.string());
        }
    }
    Upload(context.device);
}

void Texture::Unload() {
    if (GetLoadState() == ResourceLoadState::Unloaded) return;
    m_GPUView = nullptr;
    m_GPUData = nullptr;
    // If a decode is in flight the worker will still finish and publish Staged;
    // that is benign: the CPU data stays, the GPU side remains released.
    SetLoadState(ResourceLoadState::Unloaded);
}
}  // namespace hitagi::asset
