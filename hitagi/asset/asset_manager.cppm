export module asset:manager;
import std;
import core;
import gfx;
import utils;
import :resource;
import :image_codec;
import :texture;
import :material;
import :mesh;
import :camera;
import :light;
import :scene;

export namespace hitagi::asset {

inline auto is_cooked_scene_path(const std::filesystem::path& path) noexcept -> bool {
    auto extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".hcscene" || extension == ".hitagiscene";
}

class AssetManager final : public core::RuntimeModule {
public:
    // Both services must outlive this manager and every texture it creates: lazily
    // loaded textures read through `file_io` and decode on `job_system`.
    AssetManager(core::FileIOManager& file_io, core::JobSystem& job_system, std::filesystem::path asset_root_path = {});
    ~AssetManager() final;

    inline auto GetAssetRootPath() const noexcept -> const std::filesystem::path& {
        return m_AssetRootPath;
    }

    // `asset_root_path` overrides the manager-wide root for resolving the
    // scene's relative texture/shader references; empty keeps the default.
    auto ImportScene(const std::filesystem::path& path, const std::filesystem::path& asset_root_path = {}) -> std::shared_ptr<Scene>;
    auto ImportTexture(const std::filesystem::path& path) -> std::shared_ptr<Texture>;
    auto ImportMaterial(const std::filesystem::path& path) -> std::shared_ptr<Material>;

    auto LoadCookedMaterial(std::span<const std::byte> data, const std::filesystem::path& asset_root_path = {}) -> std::shared_ptr<Material>;
    auto LoadCookedMaterial(const core::Buffer& data, const std::filesystem::path& asset_root_path = {}) -> std::shared_ptr<Material>;
    auto LoadCookedScene(std::span<const std::byte> data, const std::filesystem::path& asset_root_path = {}) -> std::shared_ptr<Scene>;
    auto LoadCookedScene(const core::Buffer& data, const std::filesystem::path& asset_root_path = {}) -> std::shared_ptr<Scene>;

    class AssetLoadToken {
    public:
        AssetLoadToken();

        void RequestCancel() const noexcept;
        auto IsCancellationRequested() const noexcept -> bool;

    private:
        std::shared_ptr<std::atomic_bool> m_CancelRequested;
    };

    template <typename T>
    struct AssetLoadJob {
        std::shared_future<T> future;
        AssetLoadToken        token;

        void RequestCancel() const noexcept { token.RequestCancel(); }
        auto IsCancellationRequested() const noexcept -> bool { return token.IsCancellationRequested(); }
        auto IsValid() const noexcept -> bool { return future.valid(); }
        auto Get() const -> T { return future.get(); }
    };

    auto ImportTextureAsync(const std::filesystem::path& path, AssetLoadToken token = {}) -> AssetLoadJob<std::shared_ptr<Texture>>;

    // Identity/dedup entry point: returns the live texture registered under the
    // same (normalized) path, or creates and registers a new lazy-loading one.
    // The registry holds weak references only; ownership stays with the caller
    // chain (Scene -> Mesh -> Material -> Texture).
    auto AcquireTexture(const std::filesystem::path& path, std::string_view name = {}) -> std::shared_ptr<Texture>;

    // Registration only records weak references for lookup; it never extends
    // the lifetime of the asset.
    void AddScene(std::shared_ptr<Scene> scene);
    void AddTexture(std::shared_ptr<Texture> texture);
    void AddMaterial(std::shared_ptr<Material> material);

    auto GetMaterial(std::string_view name) -> std::shared_ptr<Material>;
    auto FindResource(const utils::UUID& uuid) -> std::shared_ptr<Resource>;

    // Releases the GPU residency of everything the scene references (meshes,
    // materials, pipelines) and drops its registry entry. CPU data stays so the
    // scene can be re-loaded cheaply. Callers must also invalidate renderer-side
    // caches (IRenderer::InvalidateResources) before the next frame.
    void UnloadScene(const std::shared_ptr<Scene>& scene);

    void WaitForAsyncJobs() noexcept;

private:
    void TrackAsyncJob(std::shared_future<void> completion);
    void RegisterResource(const std::shared_ptr<Resource>& resource);

    core::FileIOManager&  m_FileIO;
    core::JobSystem&      m_JobSystem;
    std::filesystem::path m_AssetRootPath;

    struct Registry {
        std::pmr::map<std::filesystem::path, std::weak_ptr<Texture>>            textures_by_path;
        std::pmr::map<std::pmr::string, std::weak_ptr<Material>, std::less<>>   materials_by_name;
        std::pmr::map<utils::UUID, std::weak_ptr<Resource>>                     resources_by_uuid;
    } m_Registry;
    std::mutex m_AssetsMutex;

    std::pmr::vector<std::shared_future<void>> m_AsyncJobs;
    std::mutex                                 m_AsyncJobsMutex;
};

}  // namespace hitagi::asset
