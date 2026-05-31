#include <spdlog/spdlog.h>

import std;
import app;
import asset;
import core;
import gfx;
import math;
import render;
import utils;

namespace {

using namespace hitagi;

struct Options {
    std::filesystem::path scene_path = "assets/test/test.usda";
    std::filesystem::path output     = "build/scene_capture.png";
    std::filesystem::path asset_root = "assets";
    std::pmr::string      backend    = "DX12";
    std::pmr::string      selection_case;
    std::uint32_t         width      = 1280;
    std::uint32_t         height     = 720;
    std::uint32_t         frames     = 1;
    bool                  require_color_output = false;
    bool                  selection_fixture = false;
};

struct PixelStats {
    std::size_t sampled_pixels = 0;
    std::size_t non_black      = 0;
    std::size_t color_pixels   = 0;
};

struct CameraSetup {
    asset::Camera camera = asset::Camera(asset::Camera::Parameters{});
    math::mat4f   transform = math::mat4f::identity();
    math::AABBf   bounds;
    std::size_t   visible_meshes = 0;
};

struct SelectionFixture {
    std::shared_ptr<asset::Scene> scene;
    asset::Camera                 camera = asset::Camera(asset::Camera::Parameters{});
    math::mat4f                   camera_transform = math::mat4f::identity();
    render::EditorSelectionDesc   selection;
};

enum struct Axis : std::uint8_t {
    X,
    Y,
    Z,
};

auto AxisVector(Axis axis) noexcept -> math::vec3f {
    switch (axis) {
        case Axis::X:
            return {1.0f, 0.0f, 0.0f};
        case Axis::Y:
            return {0.0f, 1.0f, 0.0f};
        case Axis::Z:
            return {0.0f, 0.0f, 1.0f};
    }
    return {0.0f, 0.0f, 1.0f};
}

auto AxisValue(const math::vec3f& v, Axis axis) noexcept -> float {
    switch (axis) {
        case Axis::X:
            return v.x;
        case Axis::Y:
            return v.y;
        case Axis::Z:
            return v.z;
    }
    return v.z;
}

void SetAxisValue(math::vec3f& v, Axis axis, float value) noexcept {
    switch (axis) {
        case Axis::X:
            v.x = value;
            return;
        case Axis::Y:
            v.y = value;
            return;
        case Axis::Z:
            v.z = value;
            return;
    }
}

auto PickVerticalAxis(const math::vec3f& extents) noexcept -> Axis {
    if (extents.y <= extents.x && extents.y <= extents.z) return Axis::Y;
    if (extents.z <= extents.x && extents.z <= extents.y) return Axis::Z;
    return Axis::X;
}

auto ParseBackend(std::string_view backend) -> gfx::Device::Type {
    if (backend == "DX12" || backend == "dx12") return gfx::Device::Type::DX12;
    if (backend == "Vulkan" || backend == "vulkan") return gfx::Device::Type::Vulkan;
    return gfx::Device::Type::DX12;
}

auto ParseOptions(int argc, char** argv) -> Options {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            options.output = argv[++i];
        } else if (arg == "--asset-root" && i + 1 < argc) {
            options.asset_root = argv[++i];
        } else if (arg == "--backend" && i + 1 < argc) {
            options.backend = argv[++i];
        } else if (arg == "--selection-case" && i + 1 < argc) {
            options.selection_case = argv[++i];
        } else if (arg == "--selection-fixture") {
            options.selection_fixture = true;
        } else if (arg == "--width" && i + 1 < argc) {
            options.width = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            options.height = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--frames" && i + 1 < argc) {
            options.frames = std::max(1ul, std::stoul(argv[++i]));
        } else if (arg == "--require-color-output") {
            options.require_color_output = true;
        } else if (arg == "--help" || arg == "-h") {
            std::println("usage: scene-capture [scene-path] [--out build/sponza_capture.png] [--backend DX12|Vulkan] [--selection-fixture --selection-case selected|occluded|multi|hover|none] [--frames N] [--require-color-output]");
            std::exit(0);
        } else {
            options.scene_path = std::string(arg);
        }
    }
    return options;
}

auto CreateFixtureCubeMesh(asset::AssetManager& assets) -> std::shared_ptr<asset::Mesh> {
    auto mesh = asset::MeshFactory::Cube();

    mesh->vertices->Modify<asset::VertexAttribute::Normal>([](std::span<math::vec3f> normals) {
        constexpr std::array values = {
            math::vec3f{-1.0f, -1.0f, -1.0f},
            math::vec3f{+1.0f, -1.0f, -1.0f},
            math::vec3f{+1.0f, -1.0f, +1.0f},
            math::vec3f{-1.0f, -1.0f, +1.0f},
            math::vec3f{-1.0f, +1.0f, -1.0f},
            math::vec3f{+1.0f, +1.0f, -1.0f},
            math::vec3f{+1.0f, +1.0f, +1.0f},
            math::vec3f{-1.0f, +1.0f, +1.0f},
        };
        for (std::size_t i = 0; i < normals.size(); ++i) {
            normals[i] = math::normalize(values[i]);
        }
    });
    mesh->vertices->Modify<asset::VertexAttribute::UV0>([](std::span<math::vec2f> uv) {
        for (auto& value : uv) value = {};
    });

    auto material = assets.GetMaterial("Phong");
    if (material && !mesh->sub_meshes.empty()) {
        mesh->sub_meshes.front().material_instance = material->CreateInstance();
    }
    return mesh;
}

auto CreateSelectionFixture(asset::AssetManager& assets, std::string_view selection_case, std::uint32_t width, std::uint32_t height) -> SelectionFixture {
    auto scene = std::make_shared<asset::Scene>("selection-outline-fixture");
    auto cube  = CreateFixtureCubeMesh(assets);
    auto root  = scene->GetRootEntity();

    const auto a = scene->CreateMeshEntity(cube, math::translate(math::vec3f{0.0f, 0.0f, 0.0f}), root, "selected-cube");
    const auto c = scene->CreateMeshEntity(cube, math::translate(math::vec3f{1.55f, 0.0f, 0.0f}), root, "side-cube");
    if (selection_case == "occluded") {
        scene->CreateMeshEntity(cube, math::translate(math::vec3f{0.0f, -0.58f, 0.0f}) * math::scale(math::vec3f{0.82f, 0.82f, 0.82f}), root, "occluder-cube");
    }
    scene->Update();

    render::EditorSelectionDesc selection{
        .enabled = selection_case != "none" && selection_case != "no_selection",
    };
    if (selection.enabled) {
        selection.items.emplace_back(render::EditorSelectionItem{
            .entity       = a,
            .visual       = render::EditorSelectionVisual::Selected,
            .selection_id = 1,
        });
        if (selection_case == "multi") {
            selection.items.emplace_back(render::EditorSelectionItem{
                .entity       = c,
                .visual       = render::EditorSelectionVisual::Selected,
                .selection_id = 2,
            });
        } else if (selection_case == "hover") {
            selection.items.emplace_back(render::EditorSelectionItem{
                .entity       = c,
                .visual       = render::EditorSelectionVisual::Hovered,
                .selection_id = 2,
            });
        }
    }

    auto camera = asset::Camera(asset::Camera::Parameters{
        .aspect         = static_cast<float>(width) / static_cast<float>(height),
        .near_clip      = 0.01f,
        .far_clip       = 50.0f,
        .horizontal_fov = static_cast<float>(52.0_deg),
        .eye            = {0.25f, -5.0f, 1.15f},
        .look_dir       = math::normalize(math::vec3f{-0.08f, 1.0f, -0.18f}),
        .up             = {0.0f, 0.0f, 1.0f},
    });

    return {
        .scene     = scene,
        .camera    = camera,
        .selection = std::move(selection),
    };
}

auto ComputeSceneBounds(asset::Scene& scene) -> math::AABBf {
    math::AABBf bounds;
    scene.Update();
    for (const auto entity : scene.GetMeshEntities()) {
        const auto mesh = entity.Get<asset::MeshComponent>().mesh;
        if (mesh == nullptr || !mesh->aabb.Valid()) continue;

        const auto world_aabb = math::transform_aabb(entity.Get<asset::Transform>().world_matrix, mesh->aabb);
        bounds.Expand(world_aabb.min_point);
        bounds.Expand(world_aabb.max_point);
    }
    return bounds;
}

auto CountVisibleMeshes(asset::Scene& scene, const asset::Camera& camera, math::mat4f camera_transform) -> std::size_t {
    const math::vec3f global_eye      = (camera_transform * math::vec4f(camera.parameters.eye, 1.0f)).xyz;
    const math::vec3f global_look_dir = (camera_transform * math::vec4f(camera.parameters.look_dir, 0.0f)).xyz;
    const math::vec3f global_up       = (camera_transform * math::vec4f(camera.parameters.up, 0.0f)).xyz;
    const auto        view            = math::look_at(global_eye, global_look_dir, global_up);
    const auto        projection      = math::perspective(camera.parameters.horizontal_fov, camera.parameters.aspect, camera.parameters.near_clip, camera.parameters.far_clip);
    const auto        frustum         = math::extract_frustum(projection * view);

    std::size_t visible_meshes = 0;
    for (const auto entity : scene.GetMeshEntities()) {
        const auto mesh = entity.Get<asset::MeshComponent>().mesh;
        if (mesh == nullptr || !mesh->aabb.Valid()) continue;

        const auto world_aabb = math::transform_aabb(entity.Get<asset::Transform>().world_matrix, mesh->aabb);
        if (math::is_aabb_visible(frustum, world_aabb)) ++visible_meshes;
    }
    return visible_meshes;
}

auto CreateCaptureCamera(asset::Scene& scene, std::uint32_t width, std::uint32_t height) -> CameraSetup {
    const auto bounds  = ComputeSceneBounds(scene);
    const auto center  = bounds.Valid() ? bounds.Center() : math::vec3f(0.0f);
    const auto extents = bounds.Valid() ? bounds.Extents() : math::vec3f(10.0f);
    const auto radius  = std::max(1.0f, math::max(extents));
    const auto fov     = static_cast<float>(62.0_deg);
    const auto aspect  = static_cast<float>(width) / static_cast<float>(height);

    const auto up_axis = PickVerticalAxis(extents);
    const auto up      = AxisVector(up_axis);

    Axis view_axis  = Axis::Z;
    Axis width_axis = Axis::X;
    if (up_axis == Axis::Y) {
        view_axis  = extents.x > extents.z ? Axis::X : Axis::Z;
        width_axis = view_axis == Axis::X ? Axis::Z : Axis::X;
    } else if (up_axis == Axis::Z) {
        view_axis  = extents.x > extents.y ? Axis::X : Axis::Y;
        width_axis = view_axis == Axis::X ? Axis::Y : Axis::X;
    } else {
        view_axis  = extents.y > extents.z ? Axis::Y : Axis::Z;
        width_axis = view_axis == Axis::Y ? Axis::Z : Axis::Y;
    }

    const auto half_depth = AxisValue(extents, view_axis);
    const auto fit_distance  = std::max(
        AxisValue(extents, width_axis) / (std::tan(fov * 0.5f) * aspect),
        AxisValue(extents, up_axis) / std::tan(fov * 0.5f));

    auto target = center;
    SetAxisValue(target, up_axis, AxisValue(center, up_axis) + AxisValue(extents, up_axis) * 0.05f);

    const auto view_dir = math::normalize(AxisVector(view_axis) - AxisVector(width_axis) * 0.42f + up * 0.14f);
    const auto distance = std::max(fit_distance + half_depth + radius * 0.45f, radius * 2.4f);
    const auto eye      = target - view_dir * distance;
    const auto look     = math::normalize(target - eye);

    CameraSetup setup{
        .camera = asset::Camera(asset::Camera::Parameters{
            .aspect         = aspect,
            .near_clip      = std::max(0.01f, radius * 0.0005f),
            .far_clip       = distance + radius * 4.0f,
            .horizontal_fov = fov,
            .eye            = eye,
            .look_dir       = look,
            .up             = up,
        }),
        .bounds = bounds,
    };
    setup.visible_meshes = CountVisibleMeshes(scene, setup.camera, setup.transform);
    return setup;
}

auto AnalyzePixels(const core::Buffer& buffer, gfx::Format format) -> PixelStats {
    const auto pixel_size = gfx::get_format_byte_size(format);
    if (pixel_size < 3 || buffer.Empty()) return {};

    const auto pixel_count = buffer.GetDataSize() / pixel_size;
    const auto step        = std::max<std::size_t>(1, pixel_count / 16384);

    PixelStats stats;
    for (std::size_t pixel = 0; pixel < pixel_count; pixel += step) {
        const auto offset = pixel * pixel_size;
        const int  r      = std::to_integer<int>(buffer.GetData()[offset + 0]);
        const int  g      = std::to_integer<int>(buffer.GetData()[offset + 1]);
        const int  b      = std::to_integer<int>(buffer.GetData()[offset + 2]);

        ++stats.sampled_pixels;
        if (r > 8 || g > 8 || b > 8) ++stats.non_black;
        if (std::abs(r - g) > 8 || std::abs(g - b) > 8 || std::abs(r - b) > 8) ++stats.color_pixels;
    }
    return stats;
}

auto FrameOutputPath(const std::filesystem::path& output, std::uint32_t frame, std::uint32_t frames) -> std::filesystem::path {
    if (frames == 1) return output;

    auto directory = output.parent_path();
    auto stem      = output.stem().string();
    auto extension = output.extension().string();
    if (extension.empty()) extension = ".png";
    return directory / std::format("{}_{:03}{}", stem, frame, extension);
}

}  // namespace

auto main(int argc, char** argv) -> int {
    spdlog::set_level(spdlog::level::warn);

    const auto options = ParseOptions(argc, argv);
    auto memory_manager  = std::make_unique<core::MemoryManager>();
    auto file_io_manager = std::make_unique<core::FileIOManager>();
    auto job_system      = std::make_unique<core::JobSystem>();
    auto app             = Application::CreateApp(AppConfig{
                    .title           = "Hitagi Scene Capture",
                    .width           = options.width,
                    .height          = options.height,
                    .asset_root_path = options.asset_root,
                    .gfx_backend     = options.backend,
                    .log_level       = "warn",
                    .headless        = true,
                });
    auto device = gfx::create_device(ParseBackend(options.backend));
    auto assets = std::make_unique<asset::AssetManager>(options.asset_root);

    SelectionFixture fixture;
    auto scene = options.selection_fixture
                     ? std::shared_ptr<asset::Scene>{}
                     : assets->ImportScene(options.scene_path);
    if (options.selection_fixture) {
        fixture = CreateSelectionFixture(*assets, options.selection_case.empty() ? "selected" : std::string_view(options.selection_case), options.width, options.height);
        scene   = fixture.scene;
    }
    if (scene == nullptr) {
        std::println("scene: {}", options.scene_path.string());
        std::println("status: failed to import scene");
        return 1;
    }

    render::RenderRuntime   runtime(*device, *app, "SceneCapture");
    render::DefaultRenderer renderer(*device, *app, "SceneCapture");

    const auto format       = gfx::Format::R8G8B8A8_UNORM;
    auto camera_setup = options.selection_fixture ? CameraSetup{} : CreateCaptureCamera(*scene, options.width, options.height);
    auto& camera      = options.selection_fixture ? fixture.camera : camera_setup.camera;
    auto  camera_transform = options.selection_fixture ? fixture.camera_transform : camera_setup.transform;

    if (!options.output.parent_path().empty()) {
        std::filesystem::create_directories(options.output.parent_path());
    }

    asset::PngEncoder encoder;
    PixelStats        last_stats;
    bool              encoded_all = true;
    for (std::uint32_t frame = 0; frame < options.frames; ++frame) {
        const auto target_usages = options.selection_fixture
                                       ? (gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc | gfx::TextureUsageFlags::SRV)
                                       : (gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc);
        auto target = runtime.GetRenderGraph().Create(gfx::TextureDesc{
            .name        = std::pmr::string(std::format("SceneCaptureTarget_{}", frame)),
            .width       = options.width,
            .height      = options.height,
            .format      = format,
            .clear_value = math::Color{0.0f, 0.0f, 0.0f, 1.0f},
            .usages      = target_usages,
        });
        auto readback_buffer = device->CreateGPUBuffer(gfx::GPUBufferDesc{
            .name          = std::pmr::string(std::format("SceneCaptureReadbackBuffer_{}", frame)),
            .element_size  = gfx::get_format_byte_size(format),
            .element_count = static_cast<std::uint64_t>(options.width) * options.height,
            .usages        = gfx::GPUBufferUsageFlags::CopyDst | gfx::GPUBufferUsageFlags::MapRead,
        });

        auto render_context = runtime.MakeContext();
        target = renderer.Render(
            render_context,
            render::SceneView{
                .scene            = scene,
                .camera           = &camera,
                .camera_transform = camera_transform,
                .editor_selection = fixture.selection,
            },
            target);
        runtime.CopyToBuffer(target, readback_buffer);
        runtime.Tick();
        device->WaitIdle();

        auto* mapped_pixels = readback_buffer->Map();
        auto  pixels        = core::Buffer(readback_buffer->Size(), mapped_pixels);
        last_stats          = AnalyzePixels(pixels, format);

        const auto     frame_output = FrameOutputPath(options.output, frame, options.frames);
        asset::Texture image(options.width, options.height, format, pixels, frame_output.string());
        const auto     encoded = encoder.Encode(image, frame_output);
        encoded_all &= encoded;
        readback_buffer->UnMap();

        std::println("frame: {}", frame);
        std::println("output: {}", frame_output.string());
        std::println("encoded: {}", encoded);
        std::println("sampled_pixels: {}", last_stats.sampled_pixels);
        std::println("non_black_pixels: {}", last_stats.non_black);
        std::println("color_pixels: {}", last_stats.color_pixels);
    }

    std::println("scene: {}", options.scene_path.string());
    std::println("output: {}", options.output.string());
    std::println("encoded: {}", encoded_all);
    if (camera_setup.bounds.Valid()) {
        std::println("bounds_min: {} {} {}", camera_setup.bounds.min_point.x, camera_setup.bounds.min_point.y, camera_setup.bounds.min_point.z);
        std::println("bounds_max: {} {} {}", camera_setup.bounds.max_point.x, camera_setup.bounds.max_point.y, camera_setup.bounds.max_point.z);
    }
    std::println("camera_eye: {} {} {}", camera.parameters.eye.x, camera.parameters.eye.y, camera.parameters.eye.z);
    std::println("camera_look: {} {} {}", camera.parameters.look_dir.x, camera.parameters.look_dir.y, camera.parameters.look_dir.z);
    std::println("camera_near_far: {} {}", camera.parameters.near_clip, camera.parameters.far_clip);
    std::println("visible_meshes: {} / {}", camera_setup.visible_meshes, scene->GetMeshEntities().size());
    std::println("sampled_pixels: {}", last_stats.sampled_pixels);
    std::println("non_black_pixels: {}", last_stats.non_black);
    std::println("color_pixels: {}", last_stats.color_pixels);

    if (!encoded_all) {
        std::println("status: failed to encode output");
        return 2;
    }
    if (options.require_color_output && last_stats.color_pixels == 0) {
        std::println("status: failed, output has no color pixels");
        return 3;
    }

    std::println("status: ok");
    return 0;
}
