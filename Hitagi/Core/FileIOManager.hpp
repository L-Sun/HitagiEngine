#pragma once
#include <filesystem>
#include <unordered_map>

#include "IRuntimeModule.hpp"
#include "Buffer.hpp"

namespace Hitagi::Core {

class FileIOManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    const Buffer& SyncOpenAndReadBinary(const std::filesystem::path& file_path);

private:
    bool IsFileChanged(const std::filesystem::path& file_path) const;

    using PathHash = size_t;
    std::unordered_map<PathHash, std::filesystem::file_time_type> m_FileStateCache;
    std::unordered_map<PathHash, Buffer>                          m_FileCache;
    Buffer                                                        m_EmptyBuffer;
};

}  // namespace Hitagi::Core

namespace Hitagi {
extern std::unique_ptr<Core::FileIOManager> g_FileIoManager;
}