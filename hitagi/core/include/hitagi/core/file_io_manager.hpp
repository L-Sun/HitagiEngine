#pragma once
#include "runtime_module.hpp"
#include "buffer.hpp"

#include <filesystem>
#include <unordered_map>
#include <fstream>

namespace hitagi::core {

class FileIOManager : public RuntimeModule {
public:
    bool Initialize() final;
    void Finalize() final;

    inline std::string_view GetName() const noexcept final { return "FileIOManager"; }

    const Buffer& SyncOpenAndReadBinary(const std::filesystem::path& file_path);
    void          SaveBuffer(const Buffer& buffer, const std::filesystem::path& path);

private:
    bool          IsFileChanged(const std::filesystem::path& file_path) const;
    const Buffer& CacheFile(const std::filesystem::path& file_path, Buffer buffer);

    using PathHash = size_t;

    std::mutex                                                         m_CacheMutex;
    std::pmr::unordered_map<PathHash, std::filesystem::file_time_type> m_FileStateCache;
    std::pmr::unordered_map<PathHash, Buffer>                          m_FileCache;
    Buffer                                                             m_EmptyBuffer;
};

}  // namespace hitagi::core

namespace hitagi {
extern core::FileIOManager* file_io_manager;
}  // namespace hitagi