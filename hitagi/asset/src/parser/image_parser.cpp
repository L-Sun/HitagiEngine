#include <hitagi/asset/parser/image_parser.hpp>
#include <hitagi/core/file_io_manager.hpp>

namespace hitagi::asset {
auto ImageParser::Parse(const std::filesystem::path& path) -> std::shared_ptr<Texture> {
    return Parse(file_io_manager->SyncOpenAndReadBinary(path));
}
}  // namespace hitagi::asset