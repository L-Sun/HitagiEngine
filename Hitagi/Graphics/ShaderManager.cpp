#include "ShaderManager.hpp"

#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

namespace Hitagi::Graphics {
int  ShaderManager::Initialize() { return 0; }
void ShaderManager::Finalize() {
    m_VertexShaders.clear();
    m_PixelShaders.clear();
}
void ShaderManager::Tick() {}

void ShaderManager::LoadShader(std::filesystem::path shader_path, ShaderType type, std::string name) {
    auto data = g_FileIoManager->SyncOpenAndReadBinary(shader_path);
    if (data.Empty()) {
        spdlog::get("GraphicsManager")->error("[ShaderManager] Give up loading shader.");
        return;
    }
    if (name.empty()) name = shader_path.filename().string();
    switch (type) {
        case ShaderType::Vertex:
            m_VertexShaders.emplace(name, std::make_shared<VertexShader>(std::move(data)));
            break;
        case ShaderType::Pixel:
            m_PixelShaders.emplace(name, std::make_shared<PixelShader>(std::move(data)));
            break;
        case ShaderType::Compute:
            m_ComputeShaders.emplace(name, std::make_shared<ComputeShader>(std::move(data)));
        default:
            spdlog::get("GraphicsManager")->error("[ShaderManager] Unsupport shader type: {}", magic_enum::enum_name(type));
    }
}

}  // namespace Hitagi::Graphics
