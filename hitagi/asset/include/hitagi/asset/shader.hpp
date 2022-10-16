#pragma once
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/utils/private_build.hpp>

namespace hitagi::asset {

class Shader {
public:
    enum struct Type : std::uint8_t {
        Vertex,
        Pixel,
        Geometry,
        Compute,
    };

    Shader(Type type, std::filesystem::path path);

    inline const auto& GetPath() const noexcept { return m_Path; }

    // Reload shader source code
    void Reload();

    inline Type          GetType() const noexcept { return m_Type; }
    inline core::Buffer& SourceCode() noexcept { return m_TextData; }
    inline core::Buffer& Program() noexcept { return m_BinaryData; }

    // TODO save compiled result to reused
    // void SaveBinary();

private:
    Type                  m_Type;
    std::filesystem::path m_Path;
    core::Buffer          m_TextData;
    core::Buffer          m_BinaryData;
};

}  // namespace hitagi::asset