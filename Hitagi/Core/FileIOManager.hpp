#pragma once
#include <string>
#include <filesystem>
#include "IRuntimeModule.hpp"
#include "Buffer.hpp"

namespace Hitagi::Core {

class FileIOManager : public IRuntimeModule {
public:
    virtual ~FileIOManager(){};

    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    size_t GetFileSize(std::filesystem::path filePath) const;

    Buffer      SyncOpenAndReadBinary(std::filesystem::path filePath) const;
    std::string SyncOpenAndReadTextFileToString(std::filesystem::path filePath) const;

private:
    bool checkFile(const std::filesystem::path& filePath) const;
};  // namespace Hitagi

}  // namespace Hitagi::Core

namespace Hitagi {
extern std::unique_ptr<Core::FileIOManager> g_FileIOManager;
}