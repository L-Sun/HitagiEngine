#include <iostream>
#include <fstream>
#include "AssetLoader.hpp"

using namespace Hitagi;

int AssetLoader::Initialize() { return 0; }

void AssetLoader::Finalize() {}

void AssetLoader::Tick() {}

size_t AssetLoader::GetFileSize(std::filesystem::path filePath) const {
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "[Warning-AssetLoader] File does not exists." << std::endl;
        std::cerr << "                    File path:" << filePath << std::endl;
        return 0;
    }
    return std::filesystem::file_size(filePath);
}

Buffer AssetLoader::SyncOpenAndReadBinary(std::filesystem::path filePath) const {
    if (!checkFile(filePath)) return Buffer();

    Buffer        buf(std::filesystem::file_size(filePath));
    std::ifstream ifs(filePath, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(buf.GetData()), buf.GetDataSize());
    ifs.close();
    return buf;
}

std::string AssetLoader::SyncOpenAndReadTextFileToString(std::filesystem::path filePath) const {
    if (!checkFile(filePath)) return std::string();

    std::ifstream ifs(filePath);
    return std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
}

bool AssetLoader::checkFile(const std::filesystem::path& filePath) const {
    std::cout << "[AssetLoader] File: " << filePath << std::flush;
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "\n[Error-AssetLoader] File does not exists." << std::endl;
        return false;
    }
#if defined(_DEBUG)
    std::cout << " File size: " << std::filesystem::file_size(filePath) << " bytes." << std::endl;
#endif  // DEBUG
    return true;
}
