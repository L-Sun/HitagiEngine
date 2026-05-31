#include <spdlog/spdlog.h>

import std;
import core;
import asset;
import gfx;

namespace {

using namespace hitagi;

struct TextureStats {
    std::size_t sampled_pixels = 0;
    std::size_t color_pixels   = 0;
};

auto IsColorTextureSlot(std::string_view name) noexcept -> bool {
    return name == "diffuse_texture" ||
           name == "ambient_texture" ||
           name == "emissive_texture";
}

auto AnalyzeTextureColor(const asset::Texture& texture) -> TextureStats {
    if (texture.Empty()) return {};

    const auto pixel_size = gfx::get_format_byte_size(texture.Format());
    if (pixel_size < 3) return {};

    const auto data        = texture.GetData();
    const auto pixel_count = static_cast<std::size_t>(texture.Width()) * texture.Height();
    if (pixel_count == 0) return {};

    const auto step = std::max<std::size_t>(1, pixel_count / 4096);

    TextureStats stats;
    for (std::size_t pixel = 0; pixel < pixel_count; pixel += step) {
        const auto offset = pixel * pixel_size;
        if (offset + 2 >= data.size()) break;

        const int r = std::to_integer<int>(data[offset + 0]);
        const int g = std::to_integer<int>(data[offset + 1]);
        const int b = std::to_integer<int>(data[offset + 2]);

        ++stats.sampled_pixels;
        if (std::abs(r - g) > 8 || std::abs(g - b) > 8 || std::abs(r - b) > 8) {
            ++stats.color_pixels;
        }
    }
    return stats;
}

auto TexturePath(const std::shared_ptr<asset::Texture>& texture) -> std::string {
    if (texture == nullptr) return "<null>";
    if (!texture->GetPath().empty()) return texture->GetPath().string();
    return std::string(texture->GetName());
}

struct Options {
    std::filesystem::path asset_root = "assets";
    std::filesystem::path scene_path = "assets/test/test.usda";
    bool                  require_color_textures = false;
};

auto ParseOptions(int argc, char** argv) -> Options {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--asset-root" && i + 1 < argc) {
            options.asset_root = argv[++i];
        } else if (arg == "--require-color-textures") {
            options.require_color_textures = true;
        } else if (arg == "--help" || arg == "-h") {
            std::println("usage: asset-inspector [scene-path] [--asset-root assets] [--require-color-textures]");
            std::exit(0);
        } else {
            options.scene_path = std::string(arg);
        }
    }

    return options;
}

}  // namespace

auto main(int argc, char** argv) -> int {
    spdlog::set_level(spdlog::level::warn);

    const auto options = ParseOptions(argc, argv);

    auto memory_manager  = std::make_unique<core::MemoryManager>();
    auto file_io_manager = std::make_unique<core::FileIOManager>();
    auto job_system      = std::make_unique<core::JobSystem>();
    auto asset_manager   = std::make_unique<asset::AssetManager>(options.asset_root);

    auto scene = asset_manager->ImportScene(options.scene_path);
    if (scene == nullptr) {
        std::println("scene: {}", options.scene_path.string());
        std::println("status: failed to import scene");
        return 1;
    }

    std::pmr::unordered_set<asset::MaterialInstance*> material_instances;
    std::size_t                                       sub_mesh_count = 0;

    for (const auto entity : scene->GetMeshEntities()) {
        const auto mesh = entity.Get<asset::MeshComponent>().mesh;
        if (mesh == nullptr) continue;

        sub_mesh_count += mesh->sub_meshes.size();
        for (const auto& sub_mesh : mesh->sub_meshes) {
            if (sub_mesh.material_instance != nullptr) {
                material_instances.emplace(sub_mesh.material_instance.get());
            }
        }
    }

    std::size_t texture_param_count    = 0;
    std::size_t loaded_texture_count   = 0;
    std::size_t missing_texture_count  = 0;
    std::size_t color_texture_count    = 0;
    std::size_t gray_texture_count     = 0;
    std::size_t associated_slot_count  = 0;
    std::size_t associated_loaded_count = 0;

    std::pmr::vector<std::string> missing_textures;

    for (const auto* material_instance : material_instances) {
        associated_slot_count += material_instance->GetAssociatedTextures().size();
        for (const auto& texture : material_instance->GetAssociatedTextures()) {
            if (texture != nullptr && !texture->Empty()) ++associated_loaded_count;
        }

        for (const auto& parameter : material_instance->GetParameters()) {
            if (!std::holds_alternative<std::shared_ptr<asset::Texture>>(parameter.value)) continue;

            ++texture_param_count;
            const auto texture = std::get<std::shared_ptr<asset::Texture>>(parameter.value);
            if (texture != nullptr && !texture->Empty()) {
                ++loaded_texture_count;
                const auto stats = AnalyzeTextureColor(*texture);
                if (IsColorTextureSlot(parameter.name) && stats.sampled_pixels > 0) {
                    if (stats.color_pixels > stats.sampled_pixels / 100) {
                        ++color_texture_count;
                    } else {
                        ++gray_texture_count;
                    }
                }
            } else {
                ++missing_texture_count;
                if (missing_textures.size() < 12) {
                    missing_textures.emplace_back(std::format("{}: {}", parameter.name, TexturePath(texture)));
                }
            }
        }
    }

    std::println("scene: {}", options.scene_path.string());
    std::println("mesh_entities: {}", scene->GetMeshEntities().size());
    std::println("camera_entities: {}", scene->GetCameraEntities().size());
    std::println("sub_meshes: {}", sub_mesh_count);
    std::println("lights: {}", scene->GetLightEntities().size());
    std::println("material_instances: {}", material_instances.size());
    std::println("texture_parameters: {}", texture_param_count);
    std::println("loaded_texture_parameters: {}", loaded_texture_count);
    std::println("missing_texture_parameters: {}", missing_texture_count);
    std::println("associated_texture_slots: {}", associated_slot_count);
    std::println("associated_loaded_textures: {}", associated_loaded_count);
    std::println("color_texture_slots: {}", color_texture_count);
    std::println("gray_texture_slots: {}", gray_texture_count);

    for (const auto& missing : missing_textures) {
        std::println("missing_texture: {}", missing);
    }

    if (options.require_color_textures && color_texture_count == 0) {
        std::println("status: failed, no loaded color texture slots");
        return 2;
    }

    std::println("status: ok");
    return 0;
}
