#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include "IRuntimeModule.hpp"
#include "Buffer.hpp"

namespace My {

class AssetLoader : public IRuntimeMoudle {
public:
    virtual ~AssetLoader(){};

    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    enum AssetOpenMode { MY_OPEN_TEXT, MY_OPEN_BINARY };
    enum AssetSeekBase { MY_SEEK_SET, MY_SEEK_CUR, MY_SEEK_END };

    bool AddSearchPath(const char* path);
    bool RemoveSearchPath(const char* path);
    bool FileExists(const char* filePath);

    std::fstream& OpenFile(const char* name, AssetOpenMode mode,
                           std::fstream& fstrm);

    Buffer SyncOpenAndRead(const char* filePath, AssetOpenMode mode);

    size_t SyncRead(std::fstream& fstrm, Buffer& buf);

    void CloseFile(std::fstream& fstrm);

    size_t GetSize(std::fstream& fstrm);

    std::istream& Seek(std::fstream& fstrm, long offset, AssetSeekBase where);

    inline std::string SyncOpenAndReadTextFileToString(const char* fileName) {
        std::string result;

        Buffer buffer  = SyncOpenAndRead(fileName, MY_OPEN_TEXT);
        char*  content = reinterpret_cast<char*>(buffer.m_pData);

        if (content) result = std::string(std::move(content));
        return result;
    }

private:
    std::vector<std::string> m_strSearchPath;
};

extern AssetLoader* g_pAssetLoader;
}  // namespace My
