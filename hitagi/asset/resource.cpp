module;

export module asset:resource;
import std;
import core;
import gfx;
import utils;

export namespace hitagi::asset {

// Execution environment for Resource::Load. Async work (e.g. texture decode)
// is scheduled internally via core::JobSystem::Get() when available.
struct ResourceLoadContext {
    gfx::Device& device;
};

// Residency state machine shared by every resource. The Loading/Staged stages
// only occur on asynchronous paths (currently the texture decode job);
// synchronous resources go straight from Unloaded to Loaded inside Load().
// This is the single source of truth for "how resident is this resource" —
// content staleness (dirty flags, pending repacks) remains the derived class's
// own concern.
enum struct ResourceLoadState : std::uint8_t {
    Unloaded,  // no GPU data; CPU-side source may not be ready either
    Loading,   // background work in flight on a worker thread
    Staged,    // CPU-side data ready; GPU upload pending on the render thread
    Loaded,    // GPU resources ready
    Failed,    // permanent failure; Load() will not retry
};

// enable_shared_from_this lives at the root of the hierarchy on purpose: it must
// appear exactly once along an inheritance chain (a second occurrence makes the
// base ambiguous and silently breaks shared_from_this), and here every resource
// can hand out an owning handle to itself (e.g. for async load jobs).
class Resource : public std::enable_shared_from_this<Resource> {
public:
    enum struct Type : std::uint8_t {
        Texture,
        Material,
        Vertex,
        Index,
        Mesh,
        Camera,
        Light,
        Scene,
        Shader,
        RenderPipeline,
        ComputePipeline,
    };

    Resource(Type type, std::string_view name = "") : m_Name(name), m_Type(type), m_UUID(utils::UUID::Create()) {}
    Resource(const Resource& other)            = delete;
    Resource& operator=(const Resource& other) = delete;
    virtual ~Resource()                        = default;

    // Manual moves: std::atomic is not movable. Only unowned (not yet shared)
    // resources are ever moved, so a relaxed state transfer is sufficient.
    Resource(Resource&& other) noexcept
        : m_Name(std::move(other.m_Name)),
          m_Type(other.m_Type),
          m_UUID(std::move(other.m_UUID)),
          m_State(other.m_State.load(std::memory_order_relaxed)) {}
    Resource& operator=(Resource&& other) noexcept {
        if (this != std::addressof(other)) {
            m_Name = std::move(other.m_Name);
            m_Type = other.m_Type;
            m_UUID = std::move(other.m_UUID);
            m_State.store(other.m_State.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    // Callers without async needs pass a context with only the device:
    // `resource->Load({.device = device})` runs fully synchronously.
    virtual void Load(const ResourceLoadContext& context) = 0;
    virtual void Unload()                                 = 0;

    inline auto        GetType() const noexcept -> Type { return m_Type; }
    inline auto        GetName() const noexcept -> std::string_view { return m_Name; }
    inline const auto& GetUUID() const noexcept { return m_UUID; }
    inline void        SetName(std::string_view name) noexcept { m_Name = name; }

    inline auto GetLoadState() const noexcept -> ResourceLoadState { return m_State.load(std::memory_order_acquire); }
    // Settled means Load() has nothing left to do: either usable or permanently failed.
    inline auto IsSettled() const noexcept -> bool {
        const auto state = GetLoadState();
        return state == ResourceLoadState::Loaded || state == ResourceLoadState::Failed;
    }

protected:
    // release: publishes CPU-side writes made before the transition (e.g. decoded
    // image data) to threads that read the state with acquire.
    inline void SetLoadState(ResourceLoadState state) noexcept { m_State.store(state, std::memory_order_release); }

    std::pmr::string m_Name;
    Type             m_Type;
    utils::UUID      m_UUID;

private:
    std::atomic<ResourceLoadState> m_State{ResourceLoadState::Unloaded};
};

}  // namespace hitagi::asset
