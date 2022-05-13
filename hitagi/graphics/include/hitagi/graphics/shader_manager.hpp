#pragma once
#include "types.hpp"

#include <hitagi/core/file_io_manager.hpp>

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

class VertexShader : public core::Buffer {
public:
    using core::Buffer::Buffer;
};
class PixelShader : public core::Buffer {
public:
    using core::Buffer::Buffer;
};
class ComputeShader : public core::Buffer {
public:
    using core::Buffer::Buffer;
};

}  // namespace hitagi::graphics
