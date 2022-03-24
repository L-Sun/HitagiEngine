#pragma once
#include <filesystem>
#include <unordered_map>
#include <fstream>

#include "IRuntimeModule.hpp"
#include "Buffer.hpp"

namespace hitagi::core {

class FileIOManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    const Buffer& SyncOpenAndReadBinary(const std::filesystem::path& file_path);
    void          SaveBuffer(const Buffer& buffer, const std::filesystem::path& path);

private:
    bool IsFileChanged(const std::filesystem::path& file_path) const;

    using PathHash = size_t;
    std::unordered_map<PathHash, std::filesystem::file_time_type> m_FileStateCache;
    std::unordered_map<PathHash, Buffer>                          m_FileCache;
    Buffer                                                        m_EmptyBuffer;
};

}  // namespace hitagi::core

namespace hitagi {
extern std::unique_ptr<core::FileIOManager> g_FileIoManager;
}