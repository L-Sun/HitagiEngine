#pragma once
#include <string>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "IRuntimeModule.hpp"
#include "Buffer.hpp"

namespace Hitagi::Core {

class FileIOManager : public IRuntimeModule {
public:
    FileIOManager() : m_Logger(spdlog::stdout_color_st("FileIOManager")) {}
    virtual ~FileIOManager(){};

    virtual int  Initialize() { return 0; }
    virtual void Finalize() {}
    virtual void Tick() {}

    size_t GetFileSize(std::filesystem::path filePath) const;

    Buffer      SyncOpenAndReadBinary(std::filesystem::path filePath) const;
    std::string SyncOpenAndReadTextFileToString(std::filesystem::path filePath) const;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
};

}  // namespace Hitagi::Core

namespace Hitagi {
extern std::unique_ptr<Core::FileIOManager> g_FileIOManager;
}