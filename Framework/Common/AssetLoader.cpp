#include "AssetLoader.hpp"

using namespace My;

int AssetLoader::Initialize() { return 0; }

void AssetLoader::Finalize() { return m_strSearchPath.clear(); }

void AssetLoader::Tick() {}

bool AssetLoader::AddSearchPath(std::string_view path) {
    for (auto src : m_strSearchPath)
        if (src == path) return true;
    m_strSearchPath.push_back(std::string(path));
    return true;
}

bool AssetLoader::RemoveSearchPath(std::string_view path) {
    for (auto src = m_strSearchPath.begin(); src != m_strSearchPath.end();
         src++)
        if (*src == path) m_strSearchPath.erase(src);
    return true;
}

bool AssetLoader::FileExists(std::string_view filePath) {
    std::string  _filePath(filePath);
    std::fstream fstrm(_filePath);
    if (fstrm) {
        CloseFile(fstrm);
        return true;
    }
    return false;
}

std::fstream& AssetLoader::OpenFile(std::string_view name,
                                    std::fstream&    fstrm) {
    std::string upPath;
    std::string fullPath;

    for (size_t i = 0; i < 10; i++) {
        auto src     = m_strSearchPath.begin();
        bool looping = true;
        while (looping) {
            fullPath = upPath;
            if (src != m_strSearchPath.end()) {
                fullPath.append(*src + "/Asset/");
                src++;
            } else {
                fullPath.append("Asset/");
                looping = false;
            }
            fullPath.append(name);

#ifdef DEBUG
            std::cout << "Trying to open " << fullPath << std::endl;
#endif  // _DEBUG

            fstrm.open(fullPath, fstrm.in | fstrm.binary);
            if (fstrm) return fstrm;
        }
        upPath.append("../");
    }
    return fstrm;
}

Buffer AssetLoader::SyncOpenAndReadBinary(std::string_view filePath) {
    std::fstream fstrm;
    OpenFile(filePath, fstrm);
    Buffer* pBuff = nullptr;

    if (fstrm) {
        size_t length = GetSize(fstrm);

#ifdef DEBUG
        std::cout << "Read file " << filePath << ", " << length << " byte(s)"
                  << std::endl;
#endif  // _DEBUG

        pBuff = new Buffer(length);
        fstrm.read(reinterpret_cast<char*>(pBuff->GetData()), length);
        CloseFile(fstrm);
    } else {
        std::cout << "Error opening file " << filePath << std::endl;
        pBuff = new Buffer();
    }

    return *pBuff;
}

void AssetLoader::CloseFile(std::fstream& fstrm) { fstrm.close(); }

size_t AssetLoader::GetSize(std::fstream& fstrm) {
    if (!fstrm) {
        std::cout << "null file discriptor" << std::endl;
        return 0;
    }
    std::fstream::pos_type curr = fstrm.tellg();
    fstrm.seekg(0, std::fstream::end);
    size_t length = fstrm.tellg();
    fstrm.seekg(curr);
    return length;
}

size_t AssetLoader::SyncRead(std::fstream& fstrm, Buffer& buf) {
    if (!fstrm) {
        std::cout << "null file discriptor" << std::endl;
        return 0;
    }
    fstrm >> buf.GetData();
    return fstrm.tellg();
}

std::istream& AssetLoader::Seek(std::fstream& fstrm, long offset,
                                AssetSeekBase where) {
    switch (where) {
        case AssetSeekBase::MY_SEEK_SET:
            return fstrm.seekg(offset, std::fstream::beg);
        case AssetSeekBase::MY_SEEK_CUR:
            return fstrm.seekg(offset, std::fstream::cur);
        case AssetSeekBase::MY_SEEK_END:
            return fstrm.seekg(offset, std::fstream::end);
    }
}