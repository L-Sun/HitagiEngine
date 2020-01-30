#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include "IRuntimeModule.hpp"
#include "Buffer.hpp"

namespace My {

class AssetLoader : public IRuntimeModule {
public:
    virtual ~AssetLoader(){};

    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    enum struct AssetSeekBase { MY_SEEK_SET, MY_SEEK_CUR, MY_SEEK_END };

    bool AddSearchPath(std::string_view path);
    bool RemoveSearchPath(std::string_view path);
    bool FileExists(std::string_view filePath);

    std::fstream& OpenFile(std::string_view name, std::fstream& fstrm);

    Buffer SyncOpenAndReadBinary(std::string_view filePath);

    size_t SyncRead(std::fstream& fstrm, Buffer& buf);

    void CloseFile(std::fstream& fstrm);

    size_t GetSize(std::fstream& fstrm);

    std::istream& Seek(std::fstream& fstrm, long offset, AssetSeekBase where);

    inline std::string SyncOpenAndReadTextFileToString(
        std::string_view fileName) {
        std::string result;

        Buffer buffer  = SyncOpenAndReadBinary(fileName);
        char*  content = reinterpret_cast<char*>(buffer.GetData());

        if (content)
            result = std::string(std::move(content), buffer.GetDataSize());
        return result;
    }

private:
    std::vector<std::string> m_strSearchPath;
};

extern std::unique_ptr<AssetLoader> g_pAssetLoader;
}  // namespace My
