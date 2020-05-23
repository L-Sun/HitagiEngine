#pragma once
#include <unordered_map>
#include <utility>

#include <utility>

#include <utility>

#include "FileIOManager.hpp"

namespace Hitagi::Graphics {
enum struct ShaderType { VERTEX,
                         PIXEL,
                         GEOMETRY,
                         COMPUTE };

class VertexShader;
class PixelShader;
class ComputeShader;

class ShaderManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void                 LoadShader(std::filesystem::path shaderPath, ShaderType type, std::string name = "");
    const VertexShader&  GetVertexShader(const std::string& name) const { return m_VertexShaders.at(name); }
    const PixelShader&   GetPixelShader(const std::string& name) const { return m_PixelShaders.at(name); }
    const ComputeShader& GetComputeShader(const std::string& name) const { return m_ComputeShaders.at(name); }

private:
    std::unordered_map<std::string, VertexShader>  m_VertexShaders;
    std::unordered_map<std::string, PixelShader>   m_PixelShaders;
    std::unordered_map<std::string, ComputeShader> m_ComputeShaders;
};

class Shader {
    friend class ShaderManager;

public:
    Shader() = default;
    Shader(Core::Buffer data) : m_ShaderData(std::move(data)) {}

    const uint8_t* GetData() const { return m_ShaderData.GetData(); }
    size_t         GetDataSize() const { return m_ShaderData.GetDataSize(); }

protected:
    Core::Buffer m_ShaderData;
};

class VertexShader : public Shader {
public:
    using Shader::Shader;
};
class PixelShader : public Shader {
public:
    using Shader::Shader;
};
class ComputeShader : public Shader {
public:
    using Shader::Shader;
};

}  // namespace Hitagi::Graphics
