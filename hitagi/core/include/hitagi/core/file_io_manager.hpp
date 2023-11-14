#pragma once
#include <hitagi/core/runtime_module.hpp>
#include <hitagi/core/buffer.hpp>

#include <filesystem>
#include <unordered_map>

namespace hitagi::core {

class FileIOManager : public RuntimeModule {
public:
    FileIOManager() : RuntimeModule("FileIOManager") {}

    inline static auto Get() {
        return static_cast<FileIOManager*>(RuntimeModule::GetModule("FileIOManager"));
    }

    auto SyncOpenAndReadBinary(const std::filesystem::path& file_path) -> const Buffer&;
    void SaveString(std::string_view str, const std::filesystem::path& path);
    void SaveBuffer(const Buffer& buffer, const std::filesystem::path& path);
    void SaveBuffer(std::span<const std::byte> buffer, const std::filesystem::path& path);

private:
    bool          IsFileChanged(const std::filesystem::path& file_path) const;
    const Buffer& CacheFile(const std::filesystem::path& file_path, Buffer buffer);

    using PathHash = std::size_t;

    std::mutex                                                         m_CacheMutex;
    std::pmr::unordered_map<PathHash, std::filesystem::file_time_type> m_FileStateCache;
    std::pmr::unordered_map<PathHash, Buffer>                          m_FileCache;
    Buffer                                                             m_EmptyBuffer;
};

}  // namespace hitagi::core
