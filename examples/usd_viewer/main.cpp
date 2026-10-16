import std;
import engine;
import editor;
import anime_viewer;
import interop.nlohmann_json;
import interop.spdlog;
import interop.usd;

namespace game::anime_viewer {
using Json = nlohmann::json;

void WriteJson(const std::filesystem::path& path, const Json& value) {
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary);
        file << value.dump(2);
        if (!file) throw std::runtime_error("Cannot write viewer response");
    }
    std::filesystem::rename(temporary, path);
}

class Viewer final : public core::RuntimeModule {
public:
    Viewer(Engine& engine, std::filesystem::path source, std::filesystem::path bridge)
        : RuntimeModule("UsdAnimeViewer"), engine(engine), renderer(engine.ResourceLoadContext()), bridge(std::move(bridge)) {
        std::filesystem::create_directories(this->bridge / "requests");
        std::filesystem::create_directories(this->bridge / "responses");
        Load(source);
        engine.GuiManager().LoadApp("build/pocketjs/ui/usd-viewer-hud.js", "build/pocketjs/ui/usd-viewer-hud.pak",
                                    {.desktop = true, .raster_density = 2});
        engine.GuiManager().SetKeyboardNavigationEnabled(false);
        fps_sample_start = std::chrono::steady_clock::now();
    }

    void Load(const std::filesystem::path& source) {
        const auto extension = source.extension().string();
        if (extension != ".usd" && extension != ".usda" && extension != ".usdc" && extension != ".usdz")
            throw std::runtime_error("Expected a .usd, .usda, .usdc or .usdz stage");
        auto stage = pxr::UsdStage::Open(std::filesystem::absolute(source).generic_string());
        if (!stage) throw std::runtime_error("Cannot open USD stage");
        pxr::TfToken up_axis("Y");
        stage->GetMetadata(pxr::TfToken("upAxis"), &up_axis);
        const auto orientation = up_axis == pxr::TfToken("Z") ? math::rotate_x(math::deg2rad(-90.0f)) : math::mat4f::identity();
        auto       candidate   = UsdSceneImporter{}.Import(source);
        if (!candidate || candidate->GetMeshEntities().empty()) throw std::runtime_error("USD has no meshes or could not be opened");
        candidate->Update(engine.Jobs());
        std::vector<render::RenderDrawItem> new_items;
        std::size_t                         new_triangles = 0;
        AnimeMaterialAdapter material_adapter;
        std::unordered_map<const asset::Material*, std::shared_ptr<asset::Material>> adapted_materials;
        std::unordered_set<const asset::Mesh*> adapted_meshes;
        for (const auto entity : candidate->GetMeshEntities()) {
            if (!entity.Has<asset::MeshComponent>() || !entity.Has<asset::Transform>()) continue;
            auto mesh = entity.Get<asset::MeshComponent>().mesh;
            if (!mesh || mesh->Empty()) continue;
            if (adapted_meshes.insert(mesh.get()).second) {
                for (auto& submesh : mesh->sub_meshes) {
                    auto& adapted = adapted_materials[submesh.material.get()];
                    if (!adapted) adapted = material_adapter.Adapt(submesh.material.get());
                    submesh.material = adapted;
                }
            }
            for (const auto& sub : mesh->sub_meshes) new_triangles += sub.index_count / 3;
            new_items.push_back({.mesh = mesh, .transform = orientation * entity.Get<asset::Transform>().world_matrix});
        }
        if (!new_triangles) throw std::runtime_error("USD has no renderable triangles");
        math::vec3f lower{std::numeric_limits<float>::max()}, upper{std::numeric_limits<float>::lowest()};
        for (const auto& item : new_items) {
            for (const auto p : item.mesh->vertices->Span<asset::VertexAttribute::Position>()) {
                const math::vec3f position = (item.transform * math::vec4f(p, 1)).xyz;
                for (unsigned axis = 0; axis < 3; ++axis) {
                    if (!std::isfinite(position[axis])) throw std::runtime_error("Non-finite mesh position");
                    lower[axis] = std::min(lower[axis], position[axis]);
                    upper[axis] = std::max(upper[axis], position[axis]);
                }
            }
        }
        const auto  extent        = upper - lower;
        const float scale         = 1.7f / std::max({extent.x, extent.y, extent.z, 0.001f});
        const auto  center        = math::vec3f{(lower.x + upper.x) * 0.5f, lower.y, (lower.z + upper.z) * 0.5f};
        const auto  normalization = math::scale(scale) * math::translate(-center);
        for (auto& item : new_items) item.transform = normalization * item.transform;
        bounds_min     = (lower - center) * scale;
        bounds_max     = (upper - center) * scale;
        scene          = std::move(candidate);
        items          = std::move(new_items);
        triangles      = new_triangles;
        scene_path     = std::filesystem::absolute(source).generic_string();
        source_up_axis = up_axis.GetString();
        target         = {0, extent.y * scale * 0.5f, 0};
    }

    void Tick() final {
        rendered = false;
        ++frame;
        const auto& input = engine.App().GetInputManager();
        if (!engine.App().GetConfig().headless && input.GetBool(hid::VirtualKeyCode::MOUSE_R_BUTTON)) {
            yaw += input.GetFloatDelta(hid::MouseEvent::MOVE_X) * 0.006f;
            pitch = std::clamp(pitch + input.GetFloatDelta(hid::MouseEvent::MOVE_Y) * 0.006f, -0.7f, 1.3f);
        }
        if (!engine.App().GetConfig().headless)
            distance = std::clamp(distance * std::exp(-input.GetFloatDelta(hid::MouseEvent::SCROLL_Y) * 0.1f), 0.3f, 30.0f);
        PollRequest();
        if (engine.App().WindowsMinimized()) return;
        const auto         width  = engine.App().GetWindowWidth();
        const auto         height = engine.App().GetWindowHeight();
        auto               eye    = target + math::vec3f{std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)} * distance;
        render::RenderView view{.camera_position = eye, .view = math::look_at(eye, target - eye, {0, 1, 0}), .projection = math::perspective(45.0_deg, static_cast<float>(width) / height, 0.03f, 200.0f)};
        view.projection[2]            = (view.projection[2] + view.projection[3]) * 0.5f;
        auto& runtime                 = engine.RenderRuntime();
        graph_frame                   = runtime.GetRenderGraph().GetFrameIndex();
        const bool diagnostic_capture = pending.value("tool", "") == "capture";
        images                        = renderer.Build(runtime.GetRenderGraph(), items, view, width, height, light, outline, bounds_min, bounds_max, quality, diagnostic_capture);
        images.final = runtime.GetRenderGraph().MoveFrom(images.final, "Viewer HUD");
        images.stages["final"] = images.final;
        runtime.RenderGui(images.final, engine.GuiManager().GetDrawData(), false);
        runtime.ToSwapChain(images.final);
        if (!pending.empty() && pending.value("tool", "") == "capture") {
            const auto stage = pending.value("stage", "all");
            for (const auto& [name, handle] : images.stages) {
                if (stage != "all" && name != stage) continue;
                const auto& desc   = runtime.GetRenderGraph().GetResourceDesc(handle);
                auto        buffer = gfx::GPUBuffer::Create(engine.Device(), {.name   = std::pmr::string("Capture " + name),
                                                                              .size   = static_cast<std::uint64_t>(desc.width) * desc.height * 4,
                                                                              .usages = gfx::GPUBufferUsageFlags::MapRead | gfx::GPUBufferUsageFlags::CopyDst});
                runtime.CopyToBuffer(handle, buffer);
                captures.push_back({name, buffer, desc.width, desc.height, desc.format});
            }
        }
        if (!pending.empty()) graph_dot = runtime.GetRenderGraph().ToDot();
        rendered = true;
    }

    auto Diagnostics() const -> Json {
        const Json stages = {"albedo", "depth", "final", "hair_shadow", "hair_shadow_depth", "lighting_band", "material_id", "normals", "outline", "shadow", "shadow_depth", "shadow_factor", "specular", "toon", "world"};
        const auto pool   = engine.RenderRuntime().GetRenderGraph().GetTransientPoolStats();
        Json result = {{"frame", frame}, {"scene", scene_path}, {"source_up_axis", source_up_axis}, {"meshes", items.size()}, {"triangles", triangles}, {"draws_per_geometry_pass", images.draws}, {"backend", engine.App().GetConfig().gfx_backend}, {"width", engine.App().GetWindowWidth()}, {"height", engine.App().GetWindowHeight()}, {"stages", stages}, {"camera", {{"yaw", yaw}, {"pitch", pitch}, {"distance", distance}, {"target", {target.x, target.y, target.z}}}}, {"light", {light.x, light.y, light.z}}, {"outline", outline}, {"quality", {{"scale", quality.scale}, {"shadows", quality.shadows}, {"face_lighting", quality.face_lighting}, {"specular", quality.specular}}}, {"shadow_resolution", 2048}, {"character_lighting", "forward float normals; material ramps; analytic face threshold; hair-only face shadows"}, {"render_graph_frame", engine.RenderRuntime().GetRenderGraph().GetFrameIndex()}, {"transient_pool", {{"buffer_bytes", pool.buffer_bytes}, {"texture_bytes", pool.texture_bytes}, {"buffers", pool.buffer_count}, {"textures", pool.texture_count}}}, {"last_error", last_error}, {"limitations", {"static USD mesh poses", "alpha cutout; no sorted translucency", "analytic face fallback, not authored SDF", "display-referred ramps, no HDR tonemapping"}}};
        result["quality"]["asset_controls"] = quality.asset_controls;
        result["fps"] = fps;
        result["character_lighting"] = "float normals; material ramps; authored face/flow maps with analytic fallback";
        result["control_maps"] = Json::array();
        result["material_contracts"] = Json::object();
        result["material_binding_errors"] = Json::array();
        for (const auto& item : items) {
            for (const auto& submesh : item.mesh->sub_meshes) {
                if (!submesh.material) continue;
                for (const auto& pass : submesh.material->GetPasses()) {
                    const std::string contract(pass.pass_contract);
                    auto& entry = result["material_contracts"][contract];
                    if (entry.is_null()) entry = {{"draws", 0}, {"material_bytes", pass.material_data.GetDataSize()}};
                    entry["draws"] = entry["draws"].get<std::size_t>() + 1;
                }
                for (const auto& error : submesh.material->ValidateBindings())
                    result["material_binding_errors"].push_back(std::string(error));
                for (const auto& parameter : submesh.material->GetParameters()) {
                    if (parameter.name != "control_texture") continue;
                    if (auto texture = std::get_if<std::shared_ptr<asset::Texture>>(&parameter.value); texture && *texture) {
                        result["control_maps"].push_back({{"path", (*texture)->GetPath().generic_string()},
                            {"width", (*texture)->Width()}, {"height", (*texture)->Height()}});
                    }
                }
            }
        }
        return result;
    }

    void AfterFrame() {
        const auto now = std::chrono::steady_clock::now();
        if (!rendered) {
            fps_sample_start = now;
            fps_frames = 0;
        } else if (engine.RenderRuntime().GetRenderGraph().GetFrameIndex() > graph_frame) {
            ++fps_frames;
            const double elapsed = std::chrono::duration<double>(now - fps_sample_start).count();
            if (elapsed >= 0.5) {
                fps = static_cast<double>(fps_frames) / elapsed;
                engine.GuiManager().PostEvent(Json{{"type", "state"}, {"value", {{"kind", "viewer.fps"}, {"fps", fps}}}}.dump());
                fps_sample_start = now;
                fps_frames = 0;
            }
        }
        if (pending.empty() || !rendered) return;
        Json result = Diagnostics();
        try {
            if (engine.RenderRuntime().GetRenderGraph().GetFrameIndex() <= graph_frame) throw std::runtime_error("Render Graph did not execute; capture is not valid");
            Json files       = Json::object();
            Json image_sizes = Json::object();
            auto directory   = bridge / "captures" / pending.at("id").get<std::string>();
            std::filesystem::create_directories(directory);
            const auto dot_path = directory / "render-graph.dot";
            {
                std::ofstream dot(dot_path);
                dot << graph_dot;
            }
            result["render_graph_dot"] = std::filesystem::absolute(dot_path).generic_string();
            for (auto& capture : captures) {
                const auto   view   = gfx::GPUBufferView::Create(engine.Device(), engine.Bindings(), {.buffer = capture.buffer, .element_size = 4, .element_count = 0});
                const auto   mapped = view->GetMappedSpan<const std::uint32_t>();
                core::Buffer pixels(static_cast<std::size_t>(capture.buffer->Size()), reinterpret_cast<const std::byte*>(mapped.data()));
                if (capture.format == gfx::Format::D32_FLOAT) {
                    // Keep lossless raw device depth alongside the human-readable PNG.
                    engine.FileIO().SaveBuffer(pixels, directory / (capture.name + ".f32"));
                    auto values = pixels.Span<float>();
                    auto rgba   = pixels.Span<std::uint32_t>();
                    for (std::size_t i = 0; i < values.size(); ++i) {
                        const float z     = values[i];
                        const float shade = capture.name == "depth" ? (z >= 0.99999f ? 0 : std::clamp((0.03f * 200.0f / (200.0f - z * 199.97f)) / 8.0f, 0.0f, 1.0f)) : 1 - z;
                        const auto  c     = static_cast<std::uint32_t>(std::clamp(shade, 0.0f, 1.0f) * 255);
                        rgba[i]           = 0xff000000u | c | (c << 8) | (c << 16);
                    }
                }
                asset::Texture texture(capture.width, capture.height, gfx::Format::R8G8B8A8_UNORM, std::move(pixels));
                auto           png = asset::PngEncoder{}.Encode(texture);
                if (png.Empty()) throw std::runtime_error("PNG encoding failed");
                const auto path = directory / (capture.name + ".png");
                engine.FileIO().SaveBuffer(png, path);
                files[capture.name]       = std::filesystem::absolute(path).generic_string();
                image_sizes[capture.name] = {capture.width, capture.height};
            }
            if (pending.value("tool", "") == "capture" && files.empty()) throw std::runtime_error("Unknown capture stage");
            result["images"]      = files;
            result["image_sizes"] = image_sizes;
            result["ok"]          = true;
            if (!files.empty()) WriteJson(directory / "manifest.json", result);
        } catch (const std::exception& error) {
            result = {{"ok", false}, {"error", error.what()}};
        }
        WriteJson(bridge / "responses" / (pending.at("id").get<std::string>() + ".json"), result);
        pending = Json::object();
        captures.clear();
        rendered = false;
    }

private:
    void PollRequest() {
        if (!pending.empty()) return;
        for (const auto& entry : std::filesystem::directory_iterator(bridge / "requests")) {
            if (entry.path().extension() != ".json") continue;
            auto id = entry.path().stem().string();
            try {
                if (id.empty() || id.find_first_not_of("0123456789abcdef-") != std::string::npos) throw std::runtime_error("Invalid request id");
                std::ifstream input(entry.path());
                Json          request;
                input >> request;
                input.close();
                std::filesystem::remove(entry.path());
                request["id"]   = id;
                const auto tool = request.at("tool").get<std::string>();
                if (tool == "camera") {
                    const float y = request.value("yaw", yaw), p = request.value("pitch", pitch), d = request.value("distance", distance);
                    if (!std::isfinite(y) || !std::isfinite(p) || !std::isfinite(d) || d < 0.3f || d > 30 || p < -.7f || p > 1.3f) throw std::runtime_error("Invalid camera parameters");
                    auto candidate_target = target;
                    if (request.contains("target")) {
                        const auto t = request.at("target").get<std::array<float, 3>>();
                        if (!std::ranges::all_of(t, [](float v) { return std::isfinite(v) && std::abs(v) < 10000; })) throw std::runtime_error("Invalid camera target");
                        candidate_target = {t[0], t[1], t[2]};
                    }
                    yaw      = y;
                    pitch    = p;
                    distance = d;
                    target   = candidate_target;
                } else if (tool == "open")
                    Load(request.at("path").get<std::string>());
                else if (tool == "lighting") {
                    const auto v = request.at("direction").get<std::array<float, 3>>();
                    if (!std::ranges::all_of(v, [](float x) { return std::isfinite(x) && std::abs(x) <= 1000; }) || v[0] * v[0] + v[1] * v[1] + v[2] * v[2] < 0.01f) throw std::runtime_error("Invalid light direction");
                    const auto candidate = math::normalize(math::vec3f{v[0], v[1], v[2]});
                    if (std::abs(candidate.y) > 0.99f) throw std::runtime_error("Light cannot be parallel to world up");
                    const float o = request.value("outline", outline);
                    if (!std::isfinite(o) || o < 0 || o > 4) throw std::runtime_error("Outline must be between 0 and 4");
                    light   = candidate;
                    outline = o;
                } else if (tool == "quality") {
                    auto       candidate = quality;
                    const auto scale     = request.value("scale", static_cast<int>(quality.scale));
                    if (scale != 1 && scale != 2 && scale != 4) throw std::runtime_error("Render scale must be 1, 2 or 4");
                    candidate.scale         = static_cast<std::uint32_t>(scale);
                    candidate.shadows       = request.value("shadows", quality.shadows);
                    candidate.face_lighting = request.value("face_lighting", quality.face_lighting);
                    candidate.specular      = request.value("specular", quality.specular);
                    candidate.asset_controls = request.value("asset_controls", quality.asset_controls);
                    quality                 = candidate;
                } else if (tool != "capture" && tool != "diagnostics")
                    throw std::runtime_error("Unknown viewer tool");
                if (tool != "capture" && tool != "diagnostics") last_error.clear();
                pending = std::move(request);
            } catch (const std::exception& error) {
                last_error = error.what();
                std::filesystem::remove(entry.path());
                if (id.find_first_not_of("0123456789abcdef-") == std::string::npos)
                    WriteJson(bridge / "responses" / (id + ".json"), {{"ok", false}, {"error", error.what()}});
            }
            break;
        }
    }
    struct Capture {
        std::string                     name;
        std::shared_ptr<gfx::GPUBuffer> buffer;
        std::uint32_t                   width, height;
        gfx::Format                     format;
    };
    Engine&                             engine;
    ToonRenderer                        renderer;
    std::filesystem::path               bridge;
    std::shared_ptr<asset::Scene>       scene;
    std::vector<render::RenderDrawItem> items;
    std::string                         scene_path;
    std::string                         source_up_axis, last_error;
    std::pmr::string                    graph_dot;
    std::size_t                         triangles   = 0;
    std::uint64_t                       frame       = 0;
    std::uint64_t                       graph_frame = 0;
    std::chrono::steady_clock::time_point fps_sample_start;
    std::uint64_t                       fps_frames = 0;
    double                              fps = 0;
    float                               yaw = 0.13f, pitch = 0.08f, distance = 3.15f, outline = 1;
    math::vec3f                         target{0, 0.85f, 0}, light{-0.6f, 0.8f, -0.7f};
    FrameImages                         images;
    RenderSettings                      quality;
    math::vec3f                         bounds_min{}, bounds_max{};
    Json                                pending = Json::object();
    std::vector<Capture>                captures;
    bool                                rendered = false;
};
}  // namespace game::anime_viewer

int main(int argc, char** argv) {
    using namespace hitagi;
    try {
        std::filesystem::path source = "assets/anime_viewer/shino.usda", bridge = "temp/usd-viewer/live";
        std::uint64_t         frames = 0;
        bool headless = false;
        std::uint32_t         width = 1280, height = 900;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg == "--headless")
                headless = true;
            else if (arg == "--bridge" && i + 1 < argc)
                bridge = argv[++i];
            else if (arg == "--frames" && i + 1 < argc)
                frames = std::stoull(argv[++i]);
            else if (arg == "--width" && i + 1 < argc)
                width = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            else if (arg == "--height" && i + 1 < argc)
                height = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            else if (!arg.starts_with("--"))
                source = argv[i];
            else
                throw std::runtime_error("Usage: usd-viewer-game [scene.usd] [--bridge path] [--frames N] [--width W --height H] [--headless]");
        }
        if (width < 64 || height < 64 || width > 3840 || height > 2160) throw std::runtime_error("Viewport size outside [64,3840] x [64,2160]");
        Engine engine(AppConfig{.title = "Hitagi USD Anime Viewer | RMB orbit / Wheel zoom", .width = width, .height = height, .gfx_backend = "Vulkan", .headless = headless});
        auto   viewer = std::make_unique<game::anime_viewer::Viewer>(engine, source, bridge);
        auto*  game   = viewer.get();
        engine.AddSubModule(std::move(viewer));
        for (std::uint64_t i = 0; !engine.App().IsQuit() && (!frames || i < frames); ++i) {
            engine.Tick();
            game->AfterFrame();
        }
    } catch (const std::exception& error) {
        spdlog::error("USD viewer: {}", error.what());
        return 1;
    }
    return 0;
}
