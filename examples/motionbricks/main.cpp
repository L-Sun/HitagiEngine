import std;
import engine;
import viewer;
import motionbricks;
import motionbricks.recording;
import hid;
import render;
import math;

int main(int argc, char** argv) {
    using namespace hitagi;
    using namespace hitagi::examples;
    try {
        bool test          = false;
        std::string comparison_mode;
        bool side_walk = false;
        bool speed_demo=false;
        bool ability_demo=false;
        bool ability_soak=false;
        bool combo_test=false;
        std::string gait_probe;
        std::filesystem::path comparison_directory;
        bool variable_rate = false;
        bool idle_test     = false;
        bool record_test = false;
        bool walk_test = false;
        bool diagnostic_test = false;
        bool long_idle_test=false;
        int idle_stop_frame=-1;
        bool jump_test=false;
        bool turn_test=false;
        bool phase_test=false;
        bool camera_test=false;
        int preset_test=-1, preset_variant=0;
        for (int i = 1; i < argc; ++i) {
            if (std::string_view(argv[i]) == "--gait-probe" && i+1<argc) {
                gait_probe=argv[++i];
            } else if (std::string_view(argv[i]) == "--test-combo") {
                combo_test=true; ability_demo=true; test=true; comparison_mode="rigid";
            } else if (std::string_view(argv[i]) == "--test-actions-soak") {
                ability_soak=true; ability_demo=true; test=true; comparison_mode="rigid";
            } else if (std::string_view(argv[i]) == "--test-actions") {
                ability_demo=true; test=true; comparison_mode="rigid";
            } else if (std::string_view(argv[i]) == "--speed-demo") {
                speed_demo=true; side_walk=true; test=true; comparison_mode="rigid";
            } else if (std::string_view(argv[i]) == "--side-walk") {
                side_walk=true; test=true; comparison_mode="rigid";
            } else if (std::string_view(argv[i]) == "--compare-mode" && i+1<argc) {
                comparison_mode = argv[++i];
                if(comparison_mode!="velocity" && comparison_mode!="rigid") throw std::invalid_argument("compare-mode must be velocity or rigid");
                test=true;
            } else if(std::string_view(argv[i]) == "--compare-out" && i+1<argc) {
                comparison_directory=argv[++i];
            } else if (std::string_view(argv[i]) == "--test")
                test = true;
            else if (std::string_view(argv[i]) == "--test-variable-rate") {
                test          = true;
                variable_rate = true;
            } else if (std::string_view(argv[i]) == "--test-idle") {
                test      = true;
                idle_test = true;
            } else if (std::string_view(argv[i]) == "--test-diagnostics") {
                test = true; diagnostic_test = true;
            } else if (std::string_view(argv[i]) == "--test-walk") {
                test = true; walk_test = true;
            } else if (std::string_view(argv[i]) == "--test-recording") {
                test = true; record_test = true;
            } else if(std::string_view(argv[i])=="--test-camera-movement") {
                test=true; phase_test=true; camera_test=true;
            } else if(std::string_view(argv[i])=="--test-phase") {
                test=true; phase_test=true;
            } else if(std::string_view(argv[i])=="--test-turn") {
                test=true; turn_test=true;
            } else if(std::string_view(argv[i])=="--test-jump") {
                test=true; jump_test=true;
            } else if(std::string_view(argv[i])=="--idle-stop-frame" && i+1<argc) {
                test=true; long_idle_test=true; idle_stop_frame=std::stoi(argv[++i]);
            } else if(std::string_view(argv[i])=="--test-long-idle") {
                test=true; long_idle_test=true;
            } else if(std::string_view(argv[i])=="--test-preset" && i+1<argc) {
                test=true; preset_test=std::stoi(argv[++i]);
            } else if(std::string_view(argv[i])=="--variant" && i+1<argc) {
                preset_variant=std::stoi(argv[++i]);
            } else
                throw std::invalid_argument("Usage: xmake r motionbricks-demo [--test|--test-variable-rate|--test-idle|--test-recording|--test-walk|--test-diagnostics]");
        }
        if(!comparison_mode.empty()) {
            if(comparison_directory.empty()) throw std::invalid_argument("compare-out required");
            std::filesystem::create_directories(comparison_directory);
        }
        const auto capture_directory = comparison_mode.empty() ? std::filesystem::path("temp/motionbricks/runtime") : comparison_directory;
        core::Runtime runtime;
        if (test) {
            OrbitController        orbit;
            asset::CameraComponent camera;
            hid::InputState        input;
            orbit.Fit(camera, {.min_point = {-1, 0, -1}, .max_point = {1, 2, 1}});
            const float initial = orbit.LookDirection().norm();
            for (int i = 0; i < 40; ++i) {
                input.UpdateWheelState(0, 1);
                orbit.Update(camera, input, 1, 900);
                input.AdvanceFrame();
            }
            const float close = orbit.LookDirection().norm();
            for (int i = 0; i < 40; ++i) {
                input.UpdateWheelState(0, 1);
                orbit.Update(camera, input, 1, 900);
                input.AdvanceFrame();
            }
            const float closer = orbit.LookDirection().norm();
            if (close >= initial * .01f || closer >= close * .01f || camera.near_clip >= closer || !std::isfinite(closer))
                throw std::runtime_error("Close-up camera zoom/clipping regression");
            std::println("ZOOM_OK relative_distance={}", closer / initial);
        }
        motionbricks::Controller controller("assets/motionbricks/prepared/runtime.txt",
                                            "assets/motionbricks/derived/soma-fp32-trt11.plan");
        controller.PreserveTargetGroup(comparison_mode!="velocity");
        controller.EnableInteractions(test && !ability_demo);
        if(!gait_probe.empty()) controller.ConfigureGaitProbe(gait_probe,capture_directory);
        if(preset_test>=4) controller.SetupInteractionTest(preset_test,preset_variant);
        else if(preset_test>=0) {controller.SelectPreset(preset_test); for(int i=0;i<preset_variant;++i) controller.NextVariant();}
        App                      app(runtime, "MotionBricks");
        app.AddPlugin(DefaultPlugins{.runtime  = runtime,
                                     .platform = {.title = "MotionBricks | WASD move | Shift jog | R reset | F9 record", .width = 1280, .height = 900, .headless = test},
                                     .backend  = gfx::Device::Type::DX12});
        auto& world    = app.GetWorld();
        auto& platform = world.GetResource<platform::PlatformHost>();
        auto& graphics = world.GetResource<gfx::GraphicsContext>();
        app.AddPlugin(USDViewerPlugin{
            .platform      = platform,
            .resources     = {.device = *graphics.device, .queues = graphics.queues, .bindings = *graphics.bindings, .shader_compiler = graphics.compiler},
            .files         = runtime.IO(),
            .jobs          = runtime.Jobs(),
            .assets        = world.GetResource<asset::AssetService>(),
            .scene         = test && !ability_demo ? "assets/motionbricks/prepared/sour_presets.usda" : "assets/motionbricks/prepared/sour_runtime.usdc",
            .renderer      = RendererType::Toon,
            .deterministic = test});
        auto& viewer = world.GetResource<USDViewer>();
        viewer.FrameBounds({.min_point = {-.6f, 0, -.6f}, .max_point = {.6f, 1.8f, .6f}});
        if(side_walk || ability_demo) {
            viewer.FrameBounds({.min_point={-.35f,0,-.35f},.max_point={.35f,1.8f,.35f}});
            viewer.SetCameraView("right");
        }
        std::filesystem::create_directories("temp/motionbricks/runtime");
        std::ofstream trace;
        std::ofstream model_trace;
        std::ofstream presentation;
        std::ofstream gait_trace;
        std::ofstream ability_trace;
        if(ability_demo) { ability_trace.open(capture_directory/"abilities.csv");ability_trace<<std::setprecision(9); }
        std::ofstream idle_trace;
        if(long_idle_test) {
            idle_trace.open(capture_directory / "idle.csv");
            idle_trace << std::setprecision(9);
        }
        if(side_walk) {
            gait_trace.open(capture_directory / "gait.csv");
            gait_trace << "frame,side,source_root_x,source_root_y,source_root_z,source_foot_x,source_foot_y,source_foot_z,render_root_x,render_root_y,render_root_z,render_foot_x,render_foot_y,render_foot_z\n" << std::setprecision(9);
        }
        if (test) {
            presentation.open(capture_directory / "presentation.csv");
            presentation << "frame,root_x,root_y,root_z,camera_x,camera_y,camera_z,head_x,head_y,head_z\n";
            presentation << std::setprecision(9);
        }
        if (test) {
            trace.open(capture_directory / "trace.csv");
            model_trace.open(capture_directory / "model.jsonl");
            model_trace << std::setprecision(9);
        }
        if (test) trace << "frame,input_x,input_z,jog,inferences,root_x,root_y,root_z,inference_ms,min_sole,max_step\n";
        unsigned                         frame = 0;
        motionbricks::Vec camera_forward{0,0,-1};
        motionbricks::Recording recording;
        motionbricks::Vec recorded_input{};
        double recorded_dt = 0;
        bool recorded_jog = false, recorded_reset = false;
        int diagnostic_mode = test ? 2 : 0;
        bool paused = false;
        unsigned recorded_generation = 0;
        unsigned traced_generation = 0;
        std::pmr::vector<WorldOverlayLine> diagnostic_lines;
        std::array<motionbricks::Vec, 6> checkpoints{};
        motionbricks::Stats              before_reset;
        std::optional<motionbricks::Vec> previous_camera_offset;
        float                            maximum_camera_step = 0;
        std::array<motionbricks::Vec, 2> head_history{};
        double                           idle_acceleration_squared = 0;
        unsigned                         idle_samples              = 0;
        std::vector<std::array<motionbricks::Joint,5>> stable_cycle;
        float maximum_idle_drift=0, maximum_idle_rotation=0;
        unsigned idle_repeat_samples=0;
        viewer.SetSceneUpdate([&](asset::Scene& scene, double dt) {
            auto& input = platform.GetInputState();
            if(!test && !viewer.WantsTextInput()) {
                for(int p=0;p<4;++p)
                    if(input.GetBoolNew(static_cast<hid::VirtualKeyCode>(int(hid::VirtualKeyCode::KEY_1)+p))) controller.SelectPreset(p);
                if(input.GetBoolNew(hid::VirtualKeyCode::KEY_TAB)) controller.NextVariant();
                if(input.GetBoolNew(hid::VirtualKeyCode::KEY_SPACE)) controller.Interact();
                controller.Turn(float(input.GetBool(hid::VirtualKeyCode::KEY_E)-input.GetBool(hid::VirtualKeyCode::KEY_Q))*float(dt)*1.8f);
            }
            if (diagnostic_test) {
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_F2, frame == 20 || frame == 30 || frame == 40);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_F4, frame == 550 || frame == 570);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_N, frame == 560);
            }
            if (!viewer.WantsTextInput() && input.GetBoolNew(hid::VirtualKeyCode::KEY_F2)) diagnostic_mode = (diagnostic_mode + 1) % 3;
            if (!viewer.WantsTextInput() && input.GetBoolNew(hid::VirtualKeyCode::KEY_F4)) paused = !paused;
            if (record_test) input.UpdateKeyState(hid::VirtualKeyCode::KEY_F9, frame == 60 || frame == 180 || frame == 300);
            if (input.GetBoolNew(hid::VirtualKeyCode::KEY_F9)) recording.Toggle();
            if (test) {
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_W, frame >= 60 && frame < 180);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_D, frame >= 180 && frame < 300);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_S, frame >= 300 && frame < 420);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_L_SHIFT, frame >= 300 && frame < 420);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_R, frame == 540);
                if(!comparison_mode.empty() || long_idle_test || preset_test>=0 || jump_test || turn_test) input.UpdateKeyState(hid::VirtualKeyCode::KEY_R,false);
                if(preset_test>=4) {
                    input.UpdateKeyState(hid::VirtualKeyCode::KEY_W,false);
                    input.UpdateKeyState(hid::VirtualKeyCode::KEY_D,false);
                    input.UpdateKeyState(hid::VirtualKeyCode::KEY_S,false);
                    if(controller.NearbyInteraction()==preset_test && !controller.GetStats().completed_actions) controller.Interact();
                    if(preset_test==7 && frame==360) controller.Interact();
                }
                if (walk_test) {
                    input.UpdateKeyState(hid::VirtualKeyCode::KEY_W, frame >= 60 && frame < 1500);
                    input.UpdateKeyState(hid::VirtualKeyCode::KEY_D, false);
                    input.UpdateKeyState(hid::VirtualKeyCode::KEY_S, false);
                    input.UpdateKeyState(hid::VirtualKeyCode::KEY_L_SHIFT, false);
                    input.UpdateKeyState(hid::VirtualKeyCode::KEY_R, frame == 1740);
                }
            }
            if(side_walk) {
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_W,true);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_D,false);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_S,false);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_L_SHIFT,speed_demo && frame>=240 && frame<420);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_R,false);
            }
            if(phase_test) {
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_W,(frame>=60 && frame<420)||(frame>=480 && frame<600)||(frame>=960 && frame<1080));
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_D,frame>=600 && frame<720);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_S,frame>=720 && frame<840);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_L_SHIFT,(frame>=180 && frame<300)||(frame>=720 && frame<840));
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_R,false);
                if(frame==960) controller.NextVariant();
            }
            if(ability_demo) {
                const auto action_frame=(frame-1)%960+1;
                for(auto key:{hid::VirtualKeyCode::KEY_W,hid::VirtualKeyCode::KEY_D,hid::VirtualKeyCode::KEY_S,hid::VirtualKeyCode::KEY_L_SHIFT,hid::VirtualKeyCode::KEY_R}) input.UpdateKeyState(key,false);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_F,combo_test ? frame==60 || frame==80 || frame==112 || (frame>=700 && frame<760) : (action_frame>=60 && action_frame<90) || action_frame==444);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_J,!combo_test && (action_frame==240 || action_frame==420));
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_L_CTRL,!combo_test && (action_frame==660 || action_frame==70));
                viewer.SetCameraView("right");
            }
            if(idle_stop_frame>=0) {
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_W,frame>=60 && frame<idle_stop_frame);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_D,frame>=60 && frame<idle_stop_frame);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_S,false);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_L_SHIFT,frame>=60 && frame<idle_stop_frame);
                input.UpdateKeyState(hid::VirtualKeyCode::KEY_R,false);
            }
            motionbricks::Vec direction{
                float(input.GetBool(hid::VirtualKeyCode::KEY_D)) - float(input.GetBool(hid::VirtualKeyCode::KEY_A)), 0,
                float(input.GetBool(hid::VirtualKeyCode::KEY_S)) - float(input.GetBool(hid::VirtualKeyCode::KEY_W))};
            if(!test || camera_test) {
                if(camera_test) viewer.SetCameraView(frame<600 ? "front" : "right");
                auto forward=viewer.CameraLookDirection(); forward.y=0;
                if(forward.norm()>1e-8f) camera_forward=math::normalize(forward);
                const auto right=math::cross(camera_forward,motionbricks::Vec{0,1,0});
                direction=right*direction.x-camera_forward*direction.z;
            }
            if(test && preset_test>=4) direction=controller.InteractionTestDirection(preset_test);
            if(jump_test) { direction={}; if(frame==60 || frame==300) controller.Jump(); }
            if(turn_test) {direction={}; if(frame>=60 && frame<180) controller.Turn(.01f);}
            bool jog = input.GetBool(hid::VirtualKeyCode::KEY_L_SHIFT) || input.GetBool(hid::VirtualKeyCode::KEY_R_SHIFT);
            if((!test || ability_demo) && !viewer.WantsTextInput()) {
                if(input.GetBoolNew(hid::VirtualKeyCode::KEY_J)) controller.RequestAbility(1,direction);
                if(input.GetBoolNew(hid::VirtualKeyCode::KEY_F)) controller.RequestAbility(0,direction);
                if(input.GetBoolNew(hid::VirtualKeyCode::KEY_L_CTRL) || input.GetBoolNew(hid::VirtualKeyCode::KEY_R_CTRL)) controller.RequestAbility(3,direction);
            }
            if (idle_test) {
                direction = {};
                jog       = false;
            }
            if (!viewer.WantsTextInput() && input.GetBoolNew(hid::VirtualKeyCode::KEY_R)) controller.Reset();
            if (viewer.WantsTextInput()) {
                direction = {};
                jog       = false;
            }
            const double simulation_delta = variable_rate ? (frame % 2 ? 1.0 / 40.0 : 1.0 / 120.0) : dt;
            const bool single_step = !viewer.WantsTextInput() && input.GetBoolNew(hid::VirtualKeyCode::KEY_N);
            const auto previous_steps = controller.GetStats().steps;
            if (!paused || single_step)
                controller.Update(scene, paused ? 1.0 / 30 : simulation_delta, direction, jog);
            if (diagnostic_test && paused && controller.GetStats().steps != previous_steps + (single_step ? 1u : 0u))
                throw std::runtime_error("Diagnostic pause/single-step regression");
            const auto& s = controller.GetStats();
            if (test && s.inferences != traced_generation) {
                controller.WriteModelDiagnostics(model_trace);
                traced_generation = s.inferences;
            }
            if (test) {
                const std::array<unsigned, 6> samples{59, 179, 299, 419, 479, 539};
                for (std::size_t i = 0; i < samples.size(); ++i)
                    if (frame == samples[i]) checkpoints[i] = s.root;
                if (frame == (walk_test ? 1739 : 539)) before_reset = s;
                if(long_idle_test && frame==7199) before_reset=s;
                if(phase_test && frame==1199) before_reset=s;
            }
            // Follow the retargeted, interpolated display pose, not the newest
            // 30 Hz simulation sample. Both camera and skin must share a time.
            viewer.SetCameraCenter(motionbricks::Vec{s.display_root.x, .85f, s.display_root.z});
            recorded_input = direction; recorded_dt = paused ? (single_step ? 1.0 / 30 : 0) : simulation_delta; recorded_jog = jog;
            recorded_reset = input.GetBoolNew(hid::VirtualKeyCode::KEY_R);
            recording.Poll();
            viewer.SetStatus(std::format("{} | {} | {}", controller.PresetLabel(), controller.InteractionPrompt(), recording.Status()));
            diagnostic_lines.clear();
            controller.DrawInteractions([&](auto a,auto b,auto color) { diagnostic_lines.push_back({a,b,color}); });
            if (diagnostic_mode) controller.DrawDiagnostics([&](auto a, auto b, auto color) { diagnostic_lines.push_back({a,b,color}); }, diagnostic_mode == 2);
            viewer.SetWorldOverlay(diagnostic_lines, diagnostic_mode ? std::format(
                "#ffffff {} | {} | plan #{}\n"
                "#66ff99 {}\n"
                "#ffffff F attack | J jump | Ctrl roll | Air + F jump attack | F2 layers\n"
                "#ffcc33 REFERENCE: target frames (yellow); idle/seated use source loop\n"
                "#ff4db3 OUTPUT: raw model root path\n"
                "#33ff8c OUTPUT: root path after endpoint correction\n"
                "#40ccff CURRENT: SOMA before target IK (scaled to character)\n"
                "#ff5926 TARGET: actual legs/shoe soles | pelvis correction {:.1f} cm",
                controller.PresetLabel(), paused ? "PAUSED" : "LIVE", s.inferences, controller.InteractionPrompt(), s.pelvis_drop * 100) : std::format("#ffffff {}\n#66ff99 {}\n#ffffff WASD camera move | Shift run | Q/E face | 1-4 gait | Tab variant | R reset | F2 debug",controller.PresetLabel(),controller.InteractionPrompt()));
            if (ability_demo) {
                viewer.SetWorldOverlay(diagnostic_lines,std::format("#ffffff ACTION LIBRARY | {} | t={:.2f}s\n#ffcc33 {}",controller.AbilityStage(),frame/60.0,controller.InteractionPrompt()));
            } else if (!comparison_mode.empty()) {
                const auto keys = frame<60 ? "IDLE" : frame<180 ? "W: WALK" : frame<300 ? "D: TURN RIGHT" : frame<420 ? "SHIFT+S: RUN" : "RELEASE: STOP";
                viewer.SetWorldOverlay(diagnostic_lines, std::format("#ffffff {} | {} | t={:.2f}s\n#ffcc33 Same scripted controls; target placement is the only A/B change", comparison_mode=="rigid" ? "B: KEEP REFERENCE GROUP" : "A: REPLACE ROOT VELOCITY", keys, frame/60.0));
            }
            if(side_walk) {
                viewer.SetWorldOverlay(diagnostic_lines,std::format("#ffffff SIDE VIEW | {} | {:.2f}s\n#ffcc33 Targets: yellow | Raw path: pink | Corrected path: green\n#40ccff SOMA: cyan | Actual legs/contact: orange",speed_demo ? "Walk / Run transition" : controller.PresetLabel(),std::max(0.0,(double(frame)-120)/60.0)));
            }
            if (test) trace << frame << ',' << direction.x << ',' << direction.z << ',' << jog << ',' << s.inferences << ',' << s.root.x << ',' << s.root.y << ',' << s.root.z << ',' << s.inference_ms << ',' << s.minimum_sole << ',' << s.maximum_root_step << '\n';
            ++frame;
        });
        world.GetResource<render::RenderApp>().app.GetWorld().AddTask("MotionBricks.Record", [&] {
            recording.Capture(viewer, [&](std::ostream& trace, std::ostream& joints, std::ostream& model, unsigned index, double time) {
                if (!index || recorded_generation != controller.GetStats().inferences) {
                    controller.WriteModelDiagnostics(model); recorded_generation = controller.GetStats().inferences;
                }
                const auto root = controller.RenderedRoot(), camera = viewer.CameraCenter();
                trace << index << ',' << time << ',' << recorded_dt << ',' << recorded_input.x << ',' << recorded_input.z << ',' << recorded_jog << ',' << recorded_reset << ',' << controller.GetStats().inferences << ',' << root.x << ',' << root.y << ',' << root.z << ',' << camera.x << ',' << camera.y << ',' << camera.z << '\n';
                controller.WriteRenderedJoints(joints, index);
            });
        }, FrameStage::Last);
        if (test) {
            auto& render_world = world.GetResource<render::RenderApp>().app.GetWorld();
            render_world.AddTask("MotionBricks.Capture", [&] {
                const auto root=controller.RenderedRoot();
                const auto head=controller.RenderedHead();
                const auto camera=viewer.CameraCenter();
                if(side_walk) controller.WriteGaitDiagnostics(gait_trace,frame);
                if(ability_demo) controller.WriteAbilityDiagnostics(ability_trace,frame);
                if(long_idle_test) controller.WriteIdleDiagnostics(idle_trace,frame);
                if(long_idle_test && frame>1800 && controller.IsLooping()) {
                    const auto pose=controller.RenderedStabilityPose();
                    const auto period=controller.LoopPeriod()*2;
                    if(stable_cycle.size()<period) stable_cycle.push_back(pose);
                    else {
                        const auto& reference=stable_cycle[idle_repeat_samples%period];
                        for(int j=0;j<5;++j) {
                            maximum_idle_drift=std::max(maximum_idle_drift,(pose[j].position-reference[j].position).norm());
                            const auto a=pose[j].rotation,b=reference[j].rotation;
                            maximum_idle_rotation=std::max(maximum_idle_rotation,1-std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w));
                        }
                        ++idle_repeat_samples;
                    }
                }
                const motionbricks::Vec head_relative{head.x-camera.x,head.y-camera.y,head.z-camera.z};
                if(idle_test && frame>62 && frame<540) {
                    const float acceleration=(head_relative-head_history[1]*2.0f+head_history[0]).norm();
                    idle_acceleration_squared+=acceleration*acceleration;++idle_samples;
                }
                head_history[0]=head_history[1];head_history[1]=head_relative;
                const motionbricks::Vec offset{root.x-camera.x,0,root.z-camera.z};
                if(previous_camera_offset) maximum_camera_step=std::max(maximum_camera_step,(offset-*previous_camera_offset).norm());
                previous_camera_offset=offset;
                presentation<<frame<<','<<root.x<<','<<root.y<<','<<root.z<<','<<camera.x<<','<<camera.y<<','<<camera.z<<','<<head.x<<','<<head.y<<','<<head.z<<'\n';
                if(!ability_soak && gait_probe.empty() && !comparison_mode.empty() && frame%2==0 && (!side_walk || frame>120)) {
                    auto png=asset::PngEncoder{}.Encode(viewer.Capture());
                    std::ofstream file(capture_directory / std::format("compare-{:04}.png",(frame-(side_walk ? 120 : 0))/2-1),std::ios::binary);
                    file.write(reinterpret_cast<const char*>(png.GetData()),png.GetDataSize());
                    if(!file) throw std::runtime_error("Comparison capture failed");
                }
                if(comparison_mode.empty() && (frame==60||frame==180||frame==300||frame==420||frame==540||(walk_test&&(frame==900||frame==1499))||(long_idle_test&&(frame==1800||frame==7199))||(preset_test>=4 && frame>=60 && frame<=300 && frame%15==0))) {
                    auto png=asset::PngEncoder{}.Encode(viewer.Capture());
                    std::ofstream file(std::format("temp/motionbricks/runtime/frame-{}.png",frame),std::ios::binary);
                    file.write(reinterpret_cast<const char*>(png.GetData()),png.GetDataSize());
                } }, FrameStage::Last);
        }
        std::println("MotionBricks runtime: WASD move/turn, Shift jog, release keys to stop, R reset. Mouse orbits camera; wheel zooms.");
        const int result = app.Run(ability_soak ? 38400 : ability_demo ? 960 : side_walk ? (gait_probe.contains("long") ? 1920 : 720) : test ? (phase_test ? 1200 : long_idle_test ? 7200 : walk_test ? 1800 : 600) : 0);
        recording.Finish();
        if (record_test) {
            if (!std::filesystem::exists(recording.Directory() / "video.mp4") || std::filesystem::file_size(recording.Directory() / "video.mp4") < 1000)
                throw std::runtime_error("Recording MP4 missing");
            std::println("RECORDING_OK {}", recording.Directory().string());
        }
        if (!comparison_mode.empty()) {
            const auto& s=controller.GetStats();
            if(ability_demo) {
                const unsigned cycles=ability_soak ? 40 : 1;
                if(!combo_test && (s.ability_completed!=std::array<unsigned,4>{cycles,cycles,cycles,cycles} || s.strike_events!=2*cycles || s.ability_recoveries!=4*cycles || s.ability_model_targets<15*cycles || s.ability_model_frames<200*cycles || s.maximum_root_step>.25f))
                    throw std::runtime_error("Action state/event/continuity regression");
                if(combo_test && (s.ability_completed!=std::array<unsigned,4>{2,0,0,0} || s.strike_events!=4 || s.ability_recoveries!=2 || s.maximum_root_step>.25f)) throw std::runtime_error("Combo buffering/completion regression");
                std::println("ABILITIES_OK punch={} jump={} jump_attack={} roll={} strikes={} model_recoveries={} max_root_step={}",s.ability_completed[0],s.ability_completed[1],s.ability_completed[2],s.ability_completed[3],s.strike_events,s.ability_recoveries,s.maximum_root_step);
                std::println("ACTION_MODEL_OK targets={} generated_frames={}",s.ability_model_targets,s.ability_model_frames);
            }
            std::ofstream metrics(comparison_directory / "metrics.txt");
            metrics << std::format("mode={} generations={} max_root_step={} min_sole={} max_pelvis_drop={} max_pelvis_compression={} max_camera_relative_step={}\n",comparison_mode,s.inferences,s.maximum_root_step,s.minimum_sole,s.maximum_pelvis_drop,s.maximum_pelvis_compression,maximum_camera_step);
            if(gait_probe.empty()) std::println("COMPARISON_CAPTURE_OK mode={} simulation_frames={} fixed_dt=1/60", comparison_mode,frame);
            else std::println("GAIT_PROBE_OK case={} frames={} fixed_dt=1/60",gait_probe,frame);
            return result;
        }
        if (test) {
            const auto& s              = before_reset;
            const auto  reset_position = controller.GetStats().root;
            if (!phase_test && !idle_test && !walk_test && !jump_test && !turn_test && preset_test<0 && !long_idle_test && (s.inferences < 15 || s.maximum_root_step > .2f || s.minimum_sole < -.01f ||
                               // Preserve the original progress/speed ratio while
                               // walking at the reference clip's measured speed.
                               checkpoints[1].z - checkpoints[0].z > -controller.ForwardWalkSpeed()*.45f || checkpoints[2].x - checkpoints[1].x < controller.ForwardWalkSpeed()*.45f ||
                               checkpoints[3].z - checkpoints[2].z < 3.0f || (checkpoints[5] - checkpoints[4]).norm() > .03f ||
                               viewer.Snapshot().find("\"animations\":0") == std::string::npos ||
                               motionbricks::Vec{reset_position.x, 0, reset_position.z}.norm() > .03f))
                throw std::runtime_error("Runtime control/stop/continuity validation failed");
            if (maximum_camera_step > .0001f) throw std::runtime_error("Camera and rendered pelvis use different presentation times/spaces");
            if (idle_test) {
                const double rms = std::sqrt(idle_acceleration_squared / idle_samples);
                if (!std::isfinite(rms) || rms > .00006) throw std::runtime_error("Idle head high-frequency jitter regression");
                std::println("IDLE_OK head_second_difference_rms_m={}", rms);
            }
            std::println("PRESENTATION_OK max_camera_relative_step_m={}", maximum_camera_step);
            std::println("SHOES samples={} max_double_support_gap_m={}", s.double_support_samples, s.maximum_double_support_gap);
            std::println("KNEES samples={} min_forward_m={}", s.knee_samples, s.minimum_knee_forward);
            std::println("PELVIS max_drop_m={} max_compression_m={}", s.maximum_pelvis_drop, s.maximum_pelvis_compression);
            std::println("SMART_LOCOMOTION tokens={}..{} last={}", s.minimum_tokens, s.maximum_tokens, s.predicted_tokens);
            if (!idle_test && (s.minimum_tokens < 6 || s.maximum_tokens > 16 || s.predicted_tokens == 0))
                throw std::runtime_error("Smart locomotion timing contract failed");
            if (!idle_test && !jump_test && !turn_test && preset_test<4) {
                std::println("CONDITIONING min_locomotion_speed_mps={}", s.minimum_locomotion_condition_speed);
                if (s.minimum_locomotion_condition_speed < (preset_test==3 ? .05f : .7f))
                    throw std::runtime_error("Moving control selected a stopping/idle conditioning token");
            }
            if (diagnostic_test) std::println("DIAGNOSTICS_OK layer cycling, pause and 30 Hz single step");
            if(long_idle_test) {
                std::println("LONG_IDLE samples={} position_drift_m={} quaternion_error={}",idle_repeat_samples,maximum_idle_drift,maximum_idle_rotation);
                if(idle_repeat_samples<1000 || maximum_idle_drift>.001f || maximum_idle_rotation>.0001f)
                    throw std::runtime_error("Long idle pelvis/wrist/ankle drift regression");
            }
            if(preset_test>=0) {
                std::println("PRESET_OK preset={} variant={} targets={} completed={} pinned_frames={}",preset_test,preset_variant,s.action_targets,s.completed_actions,s.pinned_frames);
                if(preset_test>=4 && (s.completed_actions<(preset_test==7 ? 2u : 1u) || s.action_targets<2)) throw std::runtime_error("Interaction sequence did not complete");
                if(preset_test==6 && preset_variant<2 && s.pinned_frames==0) throw std::runtime_error("Authored hand pin was never applied");
                if(s.maximum_root_step>.25f) throw std::runtime_error("Preset root discontinuity");
            }
            if(jump_test) {
                if(s.completed_actions!=2 || s.maximum_root_step>.25f) throw std::runtime_error("Free jump completion/continuity regression");
                std::println("JUMP_OK completed={} max_step={}",s.completed_actions,s.maximum_root_step);
            }
            if(turn_test) {
                if(std::abs(controller.Facing()-1.2f)>.25f || !controller.IsLooping()) throw std::runtime_error("Stationary turn/return to idle regression");
                std::println("TURN_OK facing={}",controller.Facing());
            }
            if (s.maximum_pelvis_compression > .03001f)
                throw std::runtime_error("Foot contact forced an excessive pelvis drop");
            if (walk_test && (s.inferences < 90 || s.minimum_sole < -.01f || s.maximum_root_step > .2f))
                throw std::runtime_error("Sustained walk regression");
            if(phase_test) {
                if(s.inferences<40 || s.maximum_root_step>.2f || s.minimum_sole<-.01f || !controller.IsLooping()) throw std::runtime_error("Phase transition/stop regression");
                std::println("PHASE_CONTROL_OK walk/run/walk/stop/restart/turn/jog");
            }
            if (s.knee_samples + 2*s.airborne_render_frames < 1000 || !std::isfinite(s.minimum_knee_forward) || s.minimum_knee_forward < -.0001f)
                throw std::runtime_error("Knee bent behind the body-relative leg plane");
            if (s.double_support_samples < 50 || s.maximum_double_support_gap > .005f)
                throw std::runtime_error("Shoe heel/forefoot double support regression");
            std::println("RUNTIME_OK generations={} max_root_step={} min_sole={}", s.inferences, s.maximum_root_step, s.minimum_sole);
        }
        return result;
    } catch (const std::exception& e) {
        std::cerr << "MotionBricks: " << e.what() << '\n';
        return 1;
    }
}
