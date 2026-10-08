export module gfx.base:bindless;
import std;

export namespace hitagi::gfx {

class Device;

enum struct BindlessHandleType : std::uint32_t {
    Buffer,
    Texture,
    Sampler,
    Invalid,
};

struct BindlessHandle {
    std::uint32_t      index;
    BindlessHandleType type     = BindlessHandleType::Invalid;
    std::uint32_t      writable = 0;
    std::uint32_t      version  = 0;

    inline operator bool() const noexcept {
        return type != BindlessHandleType::Invalid;
    }
};

struct BindlessMetaInfo {
    BindlessHandle handle        = {};
    std::uint32_t  record_index  = 0;
    std::uint32_t  record_stride = 0;
};

class BindlessUtils {
public:
    static auto Create(Device& device, std::string_view name = "") -> std::unique_ptr<BindlessUtils>;
    virtual ~BindlessUtils() = default;

    virtual void DiscardBindlessHandle(BindlessHandle handle) = 0;

    inline auto GetName() const noexcept { return std::string_view(m_Name); }

protected:
    BindlessUtils(std::string_view name) : m_Name(name) {}

    std::pmr::string m_Name;
};

}  // namespace hitagi::gfx
