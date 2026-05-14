module;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

module asset;
import std;

using namespace hitagi::math;

namespace hitagi::asset {

AssetManager::AssetLoadToken::AssetLoadToken()
    : m_CancelRequested(std::make_shared<std::atomic_bool>(false)) {
}

void AssetManager::AssetLoadToken::RequestCancel() const noexcept {
    m_CancelRequested->store(true, std::memory_order_relaxed);
}

auto AssetManager::AssetLoadToken::IsCancellationRequested() const noexcept -> bool {
    return m_CancelRequested->load(std::memory_order_relaxed);
}

AssetManager::AssetManager(std::filesystem::path asset_base_path)
    : core::RuntimeModule("AssetManager"),
      m_BasePath(std::move(asset_base_path)) {
    if (core::FileIOManager::Get() == nullptr) {
        m_Logger->warn("File IO Manager is not initialized!");
    }

    m_MaterialParser = std::make_shared<MaterialJSONParser>();

    m_ImageDecoders[ImageFormat::PNG]  = std::make_shared<PngDecoder>(m_Logger);
    m_ImageDecoders[ImageFormat::JPEG] = std::make_shared<JpegDecoder>(m_Logger);
    m_ImageDecoders[ImageFormat::TGA]  = std::make_shared<TgaDecoder>(m_Logger);
    m_ImageDecoders[ImageFormat::BMP]  = std::make_shared<BmpDecoder>(m_Logger);

    m_ImageEncoders[ImageFormat::PNG] = std::make_shared<PngEncoder>(m_Logger);

    auto usd_parser = std::make_shared<UsdParser>(
        [this](auto name) { return GetMaterial(name); },
        m_Logger);
    m_SceneParsers[SceneFormat::USD]  = usd_parser;
    m_SceneParsers[SceneFormat::USDA] = usd_parser;
    m_SceneParsers[SceneFormat::USDC] = usd_parser;
    m_SceneParsers[SceneFormat::USDZ] = usd_parser;

    InitBuiltinMaterial();
}

AssetManager::~AssetManager() {
    WaitForAsyncJobs();
    Texture::DestroyDefaultTexture();
}

std::shared_ptr<Scene> AssetManager::ImportScene(const std::filesystem::path& path) {
    auto format = get_scene_format(path.extension().string());
    auto parser = m_SceneParsers[format];
    if (parser == nullptr) {
        throw std::runtime_error(std::format("Unsupported scene format: {}", path.string()));
    }

    auto scene = parser->Parse(path, path.parent_path());
    AddScene(scene);
    return scene;
}

std::shared_ptr<Texture> AssetManager::ImportTexture(const std::filesystem::path& path) {
    auto format = get_image_format(path.extension().string());
    auto image  = m_ImageDecoders[format]->Decode(path);
    AddTexture(image);
    return image;
}

std::shared_ptr<Material> AssetManager::ImportMaterial(const std::filesystem::path& path) {
    auto material = m_MaterialParser->Parse(path);
    std::scoped_lock lock(m_AssetsMutex);
    auto&& [iter, success] = m_Assets.materials.emplace(std::move(material));
    return *iter;
}

auto AssetManager::ImportSceneAsync(const std::filesystem::path& path, AssetLoadToken token) -> AssetLoadJob<std::shared_ptr<Scene>> {
    auto* job_system = core::JobSystem::Get();
    if (job_system == nullptr) {
        throw std::runtime_error("asset::AssetManager async import requires core::JobSystem");
    }

    auto promise = std::make_shared<std::promise<std::shared_ptr<Scene>>>();
    auto future  = promise->get_future().share();

    auto completion = job_system->Submit([this, path, token, promise] {
        try {
            if (token.IsCancellationRequested()) {
                promise->set_value(nullptr);
                return;
            }

            const auto format = get_scene_format(path.extension().string());
            auto       parser = m_SceneParsers[format];
            if (parser == nullptr) {
                throw std::runtime_error(std::format("Unsupported scene format: {}", path.string()));
            }

            auto scene = parser->Parse(path, path.parent_path());
            if (token.IsCancellationRequested()) {
                promise->set_value(nullptr);
                return;
            }

            AddScene(scene);
            promise->set_value(std::move(scene));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).share();
    TrackAsyncJob(std::move(completion));

    return {
        .future = std::move(future),
        .token  = std::move(token),
    };
}

auto AssetManager::ImportTextureAsync(const std::filesystem::path& path, AssetLoadToken token) -> AssetLoadJob<std::shared_ptr<Texture>> {
    auto* job_system = core::JobSystem::Get();
    if (job_system == nullptr) {
        throw std::runtime_error("asset::AssetManager async import requires core::JobSystem");
    }

    auto promise = std::make_shared<std::promise<std::shared_ptr<Texture>>>();
    auto future  = promise->get_future().share();

    auto completion = job_system->Submit([this, path, token, promise] {
        try {
            if (token.IsCancellationRequested()) {
                promise->set_value(nullptr);
                return;
            }

            const auto format  = get_image_format(path.extension().string());
            auto       texture = m_ImageDecoders[format]->Decode(path);
            if (token.IsCancellationRequested()) {
                promise->set_value(nullptr);
                return;
            }

            AddTexture(texture);
            promise->set_value(std::move(texture));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).share();
    TrackAsyncJob(std::move(completion));

    return {
        .future = std::move(future),
        .token  = std::move(token),
    };
}

auto AssetManager::ImportMaterialAsync(const std::filesystem::path& path, AssetLoadToken token) -> AssetLoadJob<std::shared_ptr<Material>> {
    auto* job_system = core::JobSystem::Get();
    if (job_system == nullptr) {
        throw std::runtime_error("asset::AssetManager async import requires core::JobSystem");
    }

    auto promise = std::make_shared<std::promise<std::shared_ptr<Material>>>();
    auto future  = promise->get_future().share();

    auto completion = job_system->Submit([this, path, token, promise] {
        try {
            if (token.IsCancellationRequested()) {
                promise->set_value(nullptr);
                return;
            }

            auto material = m_MaterialParser->Parse(path);
            if (token.IsCancellationRequested()) {
                promise->set_value(nullptr);
                return;
            }

            {
                std::scoped_lock lock(m_AssetsMutex);
                auto&& [iter, success] = m_Assets.materials.emplace(std::move(material));
                material                = *iter;
            }
            promise->set_value(std::move(material));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).share();
    TrackAsyncJob(std::move(completion));

    return {
        .future = std::move(future),
        .token  = std::move(token),
    };
}

void AssetManager::AddScene(std::shared_ptr<Scene> scene) {
    if (scene == nullptr) return;
    std::scoped_lock lock(m_AssetsMutex);
    m_Assets.scenes.emplace(std::move(scene));
}

void AssetManager::AddCamera(std::shared_ptr<Camera> camera) {
    std::scoped_lock lock(m_AssetsMutex);
    if (camera) m_Assets.cameras.emplace(std::move(camera));
}

void AssetManager::AddLight(std::shared_ptr<Light> light) {
    std::scoped_lock lock(m_AssetsMutex);
    if (light) m_Assets.lights.emplace(std::move(light));
}

void AssetManager::AddMesh(std::shared_ptr<Mesh> mesh) {
    std::scoped_lock lock(m_AssetsMutex);
    if (mesh) m_Assets.meshes.emplace(std::move(mesh));
}

void AssetManager::AddSkeleton(std::shared_ptr<Skeleton> skeleton) {
    std::scoped_lock lock(m_AssetsMutex);
    if (skeleton) m_Assets.skeletons.emplace(std::move(skeleton));
}

void AssetManager::AddTexture(std::shared_ptr<Texture> texture) {
    std::scoped_lock lock(m_AssetsMutex);
    if (texture) m_Assets.textures.emplace(std::move(texture));
}

auto AssetManager::GetMaterial(std::string_view name) -> std::shared_ptr<Material> {
    std::scoped_lock lock(m_AssetsMutex);
    auto iter = std::find_if(m_Assets.materials.begin(), m_Assets.materials.end(), [&](const std::shared_ptr<Material>& mat) {
        return mat->GetName() == name;
    });

    return iter != m_Assets.materials.end() ? *iter : nullptr;
}

void AssetManager::InitBuiltinMaterial() {
    auto material_path = m_BasePath / "materials";
    if (!std::filesystem::exists(m_BasePath / "materials")) {
        m_Logger->warn("Missing material folder: assets/materials");
        return;
    }
    for (const auto& material_file : std::filesystem::directory_iterator(material_path)) {
        if (material_file.is_regular_file() && material_file.path().extension() == ".json") {
            m_Logger->info("Load built in material: {}", material_file.path().string());
            auto material = m_MaterialParser->Parse(material_file.path());
            std::scoped_lock lock(m_AssetsMutex);
            m_Assets.materials.emplace(std::move(material));
        }
    }
}

void AssetManager::TrackAsyncJob(std::shared_future<void> completion) {
    std::scoped_lock lock(m_AsyncJobsMutex);
    std::erase_if(m_AsyncJobs, [](const auto& job) {
        return job.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
    m_AsyncJobs.emplace_back(std::move(completion));
}

void AssetManager::WaitForAsyncJobs() noexcept {
    std::pmr::vector<std::shared_future<void>> async_jobs;
    {
        std::scoped_lock lock(m_AsyncJobsMutex);
        async_jobs = std::move(m_AsyncJobs);
        m_AsyncJobs.clear();
    }

    for (const auto& job : async_jobs) {
        if (job.valid()) {
            job.wait();
        }
    }
}

}  // namespace hitagi::asset
