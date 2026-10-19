export module asset:image_codec;
import std;
import core;
import gfx;

namespace hitagi::asset {
export class Texture;

export struct ImageData {
    std::uint32_t width = 0;
    std::uint32_t height=0;
    gfx::Format   format=gfx::Format::UNKNOWN;
    core::Buffer  data={};

    explicit operator bool() const noexcept { return !data.Empty(); }
};

export class ImageCodec {
public:
    auto Decode(const core::Buffer& buffer) -> std::shared_ptr<Texture>;
    auto Encode(const Texture& texture) -> core::Buffer;

    virtual auto DecodeImageData(const core::Buffer& buffer) -> ImageData = 0;
    virtual auto EncodeImageData(const ImageData& image_data) -> core::Buffer = 0;
    virtual ~ImageCodec() = default;
};

export class BmpDecoder : public ImageCodec {
public:
    auto DecodeImageData(const core::Buffer& buffer) -> ImageData final;
    auto EncodeImageData(const ImageData& image_data) -> core::Buffer final;
};

export class JpegDecoder : public ImageCodec {
public:
    auto DecodeImageData(const core::Buffer& buffer) -> ImageData final;
    auto EncodeImageData(const ImageData& image_data) -> core::Buffer final;
};

export class PngDecoder : public ImageCodec {
public:
    auto DecodeImageData(const core::Buffer& buffer) -> ImageData final;
    auto EncodeImageData(const ImageData& image_data) -> core::Buffer final;
};

export class TgaDecoder : public ImageCodec {
public:
    auto DecodeImageData(const core::Buffer& buffer) -> ImageData final;
    auto EncodeImageData(const ImageData& image_data) -> core::Buffer final;
};

export class PngEncoder {
public:
    auto Encode(const Texture& texture) -> core::Buffer;
    auto Encode(const ImageData& image_data) -> core::Buffer { return PngDecoder{}.EncodeImageData(image_data); }
};

export inline auto create_image_codec_for(const std::filesystem::path& extension) -> std::shared_ptr<ImageCodec> {
    if (extension == ".bmp") return std::make_shared<BmpDecoder>();
    if (extension == ".jpg" || extension == ".jpeg") return std::make_shared<JpegDecoder>();
    if (extension == ".png") return std::make_shared<PngDecoder>();
    if (extension == ".tga") return std::make_shared<TgaDecoder>();
    return nullptr;
}

}  // namespace hitagi::asset
