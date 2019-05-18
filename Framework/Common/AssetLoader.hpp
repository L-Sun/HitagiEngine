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

    enum AssetSeekBase { MY_SEEK_SET, MY_SEEK_CUR, MY_SEEK_END };

    bool AddSearchPath(const std::string& path);
    bool RemoveSearchPath(const std::string& path);
    bool FileExists(const std::string& filePath);

    std::fstream& OpenFile(const std::string& name, std::fstream& fstrm);

    Buffer SyncOpenAndReadBinary(const std::string& filePath);

    size_t SyncRead(std::fstream& fstrm, Buffer& buf);

    void CloseFile(std::fstream& fstrm);

    size_t GetSize(std::fstream& fstrm);

    std::istream& Seek(std::fstream& fstrm, long offset, AssetSeekBase where);

    inline std::string SyncOpenAndReadTextFileToString(
        const std::string fileName) {
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

extern AssetLoader* g_pAssetLoader;
}  // namespace My
