#pragma once
#include <filesystem>

#include "IRuntimeModule.hpp"
#include "Buffer.hpp"

namespace Hitagi::Core {

class FileIOManager : public IRuntimeModule {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    size_t GetFileSize(std::filesystem::path filePath) const;

    Buffer      SyncOpenAndReadBinary(std::filesystem::path filePath) const;
    std::string SyncOpenAndReadTextFileToString(std::filesystem::path filePath) const;
};

}  // namespace Hitagi::Core

namespace Hitagi {
extern std::unique_ptr<Core::FileIOManager> g_FileIOManager;
}