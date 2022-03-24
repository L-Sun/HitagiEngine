#pragma once
#include "FileIOManager.hpp"
#include "Types.hpp"

#include <unordered_map>

namespace hitagi::graphics {

class VertexShader;
class PixelShader;
class ComputeShader;

class ShaderManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void                          LoadShader(std::filesystem::path shader_path, ShaderType type, std::string name = "");
    std::shared_ptr<VertexShader> GetVertexShader(const std::string& name) noexcept {
        return m_VertexShaders.count(name) != 0 ? m_VertexShaders.at(name) : nullptr;
    }
    std::shared_ptr<PixelShader> GetPixelShader(const std::string& name) noexcept {
        return m_PixelShaders.count(name) != 0 ? m_PixelShaders.at(name) : nullptr;
    }
    std::shared_ptr<ComputeShader> GetComputeShader(const std::string& name) noexcept {
        return m_ComputeShaders.count(name) != 0 ? m_ComputeShaders.at(name) : nullptr;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<VertexShader>>  m_VertexShaders;
    std::unordered_map<std::string, std::shared_ptr<PixelShader>>   m_PixelShaders;
    std::unordered_map<std::string, std::shared_ptr<ComputeShader>> m_ComputeShaders;
};

class Shader {
    friend ShaderManager;

public:
    Shader() = default;
    Shader(core::Buffer data) : m_ShaderData(std::move(data)) {}

    const uint8_t* GetData() const { return m_ShaderData.GetData(); }
    size_t         GetDataSize() const { return m_ShaderData.GetDataSize(); }

protected:
    core::Buffer m_ShaderData;
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

}  // namespace hitagi::graphics
