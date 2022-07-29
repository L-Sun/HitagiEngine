#include <hitagi/resource/shader.hpp>

namespace hitagi::resource {
Shader::Shader(Type type, std::filesystem::path path)
    : m_Type(type), m_Path(std::move(path)), m_TextData(file_io_manager->SyncOpenAndReadBinary(m_Path)) {
    SetName(path.filename().string());
}

void Shader::Reload() {
    m_TextData = file_io_manager->SyncOpenAndReadBinary(m_Path);
    IncreaseVersion();
}

}  // namespace hitagi::resource