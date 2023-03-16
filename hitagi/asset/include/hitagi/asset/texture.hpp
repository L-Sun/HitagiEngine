#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/gfx/gpu_resource.hpp>
#include <hitagi/gfx/device.hpp>

#include <filesystem>

namespace hitagi::asset {
class ImageParser;

class Texture : public Resource {
public:
    Texture(std::uint32_t    width,
            std::uint32_t    height,
            gfx::Format      format = gfx::Format::R8G8B8A8_UNORM,
            core::Buffer     data   = {},
            std::string_view name   = "",
            xg::Guid         guid   = {});

    Texture(std::filesystem::path path, std::string_view name = "", xg::Guid guid = {});

    Texture(const Texture&);
    Texture& operator=(const Texture&);
    Texture(Texture&&)            = default;
    Texture& operator=(Texture&&) = default;

    static auto DefaultTexture() -> std::shared_ptr<Texture>;
    static void DestoryDefaultTexture();

    inline auto  Empty() const noexcept { return m_CPUData.Empty(); }
    inline auto  Width() const noexcept { return m_Width; }
    inline auto  Height() const noexcept { return m_Height; }
    inline auto  Format() const noexcept { return m_Format; }
    inline auto  GetData() const noexcept { return m_CPUData.Span<const std::byte>(); }
    inline auto& GetPath() const noexcept { return m_Path; }
    inline auto  GetGPUData() const noexcept { return m_GPUData; }

    bool SetPath(const std::filesystem::path& path);
    bool Load(const std::shared_ptr<ImageParser>& parser);
    void Unload();

    void InitGPUData(gfx::Device& device);

private:
    std::uint32_t m_Width = 0, m_Height = 0;
    gfx::Format   m_Format;

    std::filesystem::path m_Path;

    bool                          m_Dirty   = true;
    core::Buffer                  m_CPUData = {};
    std::shared_ptr<gfx::Texture> m_GPUData = nullptr;

    static std::shared_ptr<Texture> m_DefaultTexture;
};

}  // namespace hitagi::asset
