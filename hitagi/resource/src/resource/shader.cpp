#include <hitagi/resource/shader.hpp>

namespace hitagi::resource {
Shader::Shader(Type type, std::filesystem::path path)
    : m_Type(type), m_Path(std::move(path)), m_TextData(file_io_manager->SyncOpenAndReadBinary(m_Path)) {
}

void Shader::Reload() {
    m_TextData = file_io_manager->SyncOpenAndReadBinary(m_Path);
}

}  // namespace hitagi::resource