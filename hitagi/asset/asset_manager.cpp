module;

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
import :cooked_binary;

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

namespace hitagi::asset {

AssetManager::AssetLoadToken::AssetLoadToken()
    : m_CancelRequested(std::make_shared<std::atomic_bool>(false)) {}

void AssetManager::AssetLoadToken::RequestCancel() const noexcept {
    m_CancelRequested->store(true, std::memory_order_relaxed);
}

auto AssetManager::AssetLoadToken::IsCancellationRequested() const noexcept -> bool {
    return m_CancelRequested->load(std::memory_order_relaxed);
}

AssetManager::AssetManager(core::FileIOManager& file_io, core::JobSystem& job_system, std::filesystem::path asset_root_path)
    : core::RuntimeModule("AssetManager"),
      m_FileIO(file_io),
      m_JobSystem(job_system),
      m_AssetRootPath(std::move(asset_root_path)) {}

AssetManager::~AssetManager() {
    WaitForAsyncJobs();
    Texture::DestroyDefaultTexture();
}

auto AssetManager::ImportScene(const std::filesystem::path& path, const std::filesystem::path& asset_root_path) -> std::shared_ptr<Scene> {
    if (!is_cooked_scene_path(path)) {
        throw std::runtime_error(std::format("Unsupported scene format: {}", path.string()));
    }
    return LoadCookedScene(m_FileIO.SyncOpenAndReadBinary(path), asset_root_path);
}

auto AssetManager::ImportTexture(const std::filesystem::path& path) -> std::shared_ptr<Texture> {
    const auto codec = create_image_codec_for(path.extension());
    if (!codec) {
        throw std::runtime_error(std::format("Unsupported image format: {}", path.string()));
    }

    {
        std::scoped_lock lock(m_AssetsMutex);
        if (const auto iter = m_Registry.textures_by_path.find(path.lexically_normal()); iter != m_Registry.textures_by_path.end()) {
            if (auto texture = iter->second.lock()) return texture;
        }
    }

    auto texture = codec->Decode(m_FileIO.SyncOpenAndReadBinary(path));
    if (texture) texture->SetPath(path);  // record identity so the registry can deduplicate
    AddTexture(texture);
    return texture;
}

auto AssetManager::ImportMaterial(const std::filesystem::path& path) -> std::shared_ptr<Material> {
    return LoadCookedMaterial(m_FileIO.SyncOpenAndReadBinary(path));
}

auto AssetManager::LoadCookedMaterial(std::span<const std::byte> data, const std::filesystem::path& asset_root_path) -> std::shared_ptr<Material> {
    auto material = ParseCookedMaterial(
        data,
        asset_root_path.empty() ? m_AssetRootPath : asset_root_path,
        [this](const std::filesystem::path& path, std::string_view name) { return AcquireTexture(path, name); });
    AddMaterial(material);
    return material;
}

auto AssetManager::LoadCookedMaterial(const core::Buffer& data, const std::filesystem::path& asset_root_path) -> std::shared_ptr<Material> {
    return LoadCookedMaterial(data.Span<const std::byte>(), asset_root_path);
}

auto AssetManager::LoadCookedScene(std::span<const std::byte> data, const std::filesystem::path& asset_root_path) -> std::shared_ptr<Scene> {
    auto scene = ParseCookedScene(
        data,
        asset_root_path.empty() ? m_AssetRootPath : asset_root_path,
        [this](const std::filesystem::path& path, std::string_view name) { return AcquireTexture(path, name); });
    AddScene(scene);
    return scene;
}

auto AssetManager::LoadCookedScene(const core::Buffer& data, const std::filesystem::path& asset_root_path) -> std::shared_ptr<Scene> {
    return LoadCookedScene(data.Span<const std::byte>(), asset_root_path);
}

auto AssetManager::ImportTextureAsync(const std::filesystem::path& path, AssetLoadToken token) -> AssetLoadJob<std::shared_ptr<Texture>> {
    auto promise = std::make_shared<std::promise<std::shared_ptr<Texture>>>();
    auto future  = promise->get_future().share();

    auto completion = m_JobSystem.Submit([this, path, token, promise] {
                                    try {
                                        if (token.IsCancellationRequested()) {
                                            promise->set_value(nullptr);
                                            return;
                                        }

                                        auto texture = ImportTexture(path);
                                        promise->set_value(token.IsCancellationRequested() ? nullptr : std::move(texture));
                                    } catch (...) {
                                        promise->set_exception(std::current_exception());
                                    }
                                })
                          .share();
    TrackAsyncJob(std::move(completion));

    return {.future = std::move(future), .token = std::move(token)};
}

void AssetManager::RegisterResource(const std::shared_ptr<Resource>& resource) {
    // Caller holds m_AssetsMutex.
    std::erase_if(m_Registry.resources_by_uuid, [](const auto& entry) { return entry.second.expired(); });
    m_Registry.resources_by_uuid.insert_or_assign(resource->GetUUID(), resource);
}

void AssetManager::AddScene(std::shared_ptr<Scene> scene) {
    if (!scene) return;
    std::scoped_lock lock(m_AssetsMutex);
    RegisterResource(scene);
}

void AssetManager::AddTexture(std::shared_ptr<Texture> texture) {
    if (!texture) return;
    std::scoped_lock lock(m_AssetsMutex);
    if (!texture->GetPath().empty()) {
        m_Registry.textures_by_path.insert_or_assign(texture->GetPath().lexically_normal(), texture);
    }
    RegisterResource(texture);
}

void AssetManager::AddMaterial(std::shared_ptr<Material> material) {
    if (!material) return;
    std::scoped_lock lock(m_AssetsMutex);
    if (!material->GetName().empty()) {
        std::erase_if(m_Registry.materials_by_name, [](const auto& entry) { return entry.second.expired(); });
        m_Registry.materials_by_name.insert_or_assign(std::pmr::string(material->GetName()), material);
    }
    RegisterResource(material);
}

auto AssetManager::AcquireTexture(const std::filesystem::path& path, std::string_view name) -> std::shared_ptr<Texture> {
    if (path.empty()) return nullptr;
    const auto key = path.lexically_normal();

    std::scoped_lock lock(m_AssetsMutex);
    if (const auto iter = m_Registry.textures_by_path.find(key); iter != m_Registry.textures_by_path.end()) {
        if (auto texture = iter->second.lock()) return texture;
        m_Registry.textures_by_path.erase(iter);
    }

    auto texture = std::make_shared<Texture>(key, MakeFileImageLoader(m_FileIO, key), name, m_JobSystem.MakeSubmitter());
    m_Registry.textures_by_path.insert_or_assign(key, texture);
    RegisterResource(texture);
    return texture;
}

auto AssetManager::GetMaterial(std::string_view name) -> std::shared_ptr<Material> {
    std::scoped_lock lock(m_AssetsMutex);
    const auto       iter = m_Registry.materials_by_name.find(name);
    return iter == m_Registry.materials_by_name.end() ? nullptr : iter->second.lock();
}

auto AssetManager::FindResource(const utils::UUID& uuid) -> std::shared_ptr<Resource> {
    std::scoped_lock lock(m_AssetsMutex);
    const auto       iter = m_Registry.resources_by_uuid.find(uuid);
    return iter == m_Registry.resources_by_uuid.end() ? nullptr : iter->second.lock();
}

void AssetManager::UnloadScene(const std::shared_ptr<Scene>& scene) {
    if (!scene) return;
    scene->Unload();
    std::scoped_lock lock(m_AssetsMutex);
    m_Registry.resources_by_uuid.erase(scene->GetUUID());
}

void AssetManager::TrackAsyncJob(std::shared_future<void> completion) {
    std::scoped_lock lock(m_AsyncJobsMutex);
    std::erase_if(m_AsyncJobs, [](const auto& job) {
        return job.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
    m_AsyncJobs.emplace_back(std::move(completion));
}

void AssetManager::WaitForAsyncJobs() noexcept {
    std::pmr::vector<std::shared_future<void>> jobs;
    {
        std::scoped_lock lock(m_AsyncJobsMutex);
        jobs = std::move(m_AsyncJobs);
        m_AsyncJobs.clear();
    }
    for (const auto& job : jobs) {
        if (job.valid()) job.wait();
    }
}

}  // namespace hitagi::asset
