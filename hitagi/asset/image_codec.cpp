module;

module asset;
import :image_codec;
import std;

namespace hitagi::asset {
auto ImageDecoder::Decode(const std::filesystem::path& path) -> std::shared_ptr<Texture> {
    if (core::FileIOManager::Get())
        return Decode(core::FileIOManager::Get()->SyncOpenAndReadBinary(path));
    else
        return nullptr;
}

auto ImageEncoder::Encode(const Texture& texture, const std::filesystem::path& path) -> bool {
    auto buffer = Encode(texture);
    if (buffer.Empty()) return false;
    if (core::FileIOManager::Get()) {
        core::FileIOManager::Get()->SaveBuffer(buffer, path);
        return true;
    }
    return false;
}
}  // namespace hitagi::asset
