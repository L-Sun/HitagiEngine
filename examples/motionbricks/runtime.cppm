// SOMA tensor encoding/decoding and endpoint correction adapted from
// NVlabs/motionbricks (54fc563aeaaf819a753d72cc9fdc6713989cdd43),
// Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES, Apache-2.0.
export module motionbricks;

import std;
import math;
import asset;
import ecs;
import interop.tensorrt;

export namespace hitagi::examples::motionbricks {
using Vec      = math::vec3f;
using Quat     = math::quatf;
using Joint    = asset::LocalPose;
using SomaPose = std::array<Joint, 77>;

auto Inverse(Quat q) -> Quat { return {-q.x, -q.y, -q.z, q.w}; }
auto Rotate(Quat q, Vec v) -> Vec {
    const Vec u{q.x, q.y, q.z};
    return v + math::cross(u, math::cross(u, v) + v * q.w) * 2.0f;
}
auto Compose(const Joint& a, const Joint& b) -> Joint {
    return {.position = a.position + Rotate(a.rotation, b.position), .rotation = a.rotation * b.rotation};
}
auto Relative(const Joint& parent, const Joint& child) -> Joint {
    return {.position = Rotate(Inverse(parent.rotation), child.position - parent.position),
            .rotation = Inverse(parent.rotation) * child.rotation};
}
auto Mix(Quat a, Quat b, float t) -> Quat {
    return asset::BlendLocalPose(Joint{.rotation = a}, Joint{.rotation = b}, t).rotation;
}
auto Yaw(float angle) -> Quat { return math::axis_angle_to_quaternion(Vec{0, 1, 0}, angle); }
auto Arc(Vec a, Vec b) -> Quat {
    if (a.norm() < 1e-7f || b.norm() < 1e-7f) return Quat::identity();
    a             = math::normalize(a);
    b             = math::normalize(b);
    const float d = std::clamp(math::dot(a, b), -1.0f, 1.0f);
    if (d < -.9999f) {
        Vec axis = math::cross(a, Vec{1, 0, 0});
        if (axis.norm() < .01f) axis = math::cross(a, Vec{0, 0, 1});
        return math::axis_angle_to_quaternion(axis, std::numbers::pi_v<float>);
    }
    const Vec c = math::cross(a, b);
    Quat      q{c.x, c.y, c.z, 1 + d};
    return {q.x / q.norm(), q.y / q.norm(), q.z / q.norm(), q.w / q.norm()};
}
auto Rotation6D(const float* six) -> Quat {
    Vec x{six[0], six[1], six[2]}, y{six[3], six[4], six[5]};
    if (x.norm() < 1e-5f || math::cross(x, y).norm() < 1e-5f) throw std::runtime_error("Degenerate model rotation");
    x     = math::normalize(x);
    Vec z = math::normalize(math::cross(x, y));
    y     = math::cross(z, x);
    // Stable matrix-to-quaternion conversion, including 180-degree turns.
    const std::array<Vec, 3> rows{Vec{x.x, y.x, z.x}, Vec{x.y, y.y, z.y}, Vec{x.z, y.z, z.z}};
    const auto&              m = rows;
    Quat                     q{};
    const float              trace = m[0][0] + m[1][1] + m[2][2];
    if (trace > 0) {
        const float s = std::sqrt(trace + 1) * 2;
        q             = {(m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s, .25f * s};
    } else {
        int i = m[1][1] > m[0][0] ? 1 : 0;
        if (m[2][2] > m[i][i]) i = 2;
        const int   j = (i + 1) % 3, k = (i + 2) % 3;
        const float s = std::sqrt(1 + m[i][i] - m[j][j] - m[k][k]) * 2;
        q[i]          = .25f * s;
        q[j]          = (m[i][j] + m[j][i]) / s;
        q[k]          = (m[i][k] + m[k][i]) / s;
        q.w           = (m[k][j] - m[j][k]) / s;
    }
    const float n = q.norm();
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}

void Cuda(interop::cudaError_t result) {
    if (result != interop::cudaSuccess) throw std::runtime_error(interop::cudaGetErrorString(result));
}
class Inference {
    class Logger final : public interop::trt::ILogger {
        void log(Severity level, const char* message) noexcept override {
            if (level <= Severity::kWARNING) {
                try {
                    std::cerr << "TensorRT: " << message << '\n';
                } catch (...) {
                }
            }
        }
    } logger;
    struct Allocation {
        void* data = nullptr;
        ~Allocation() {
            if (data) interop::cudaFree(data);
        }
    };
    struct Stream {
        interop::cudaStream_t value{};
        Stream() { Cuda(interop::cudaStreamCreate(&value)); }
        ~Stream() {
            if (value) interop::cudaStreamDestroy(value);
        }
    } stream;
    std::unique_ptr<interop::trt::IRuntime>          runtime;
    std::unique_ptr<interop::trt::ICudaEngine>       engine;
    std::unique_ptr<interop::trt::IExecutionContext> context;
    std::array<Allocation, 9>                        buffers;
    static constexpr std::array<const char*, 9>      names{
        "global_root_values", "has_global_root_values", "local_root_values", "has_local_root_values",
        "local_poses", "has_local_poses", "allowed_pred_num_tokens", "pred_global_motions", "pred_num_tokens"};
    static constexpr std::array<std::size_t, 9> sizes{40, 8, 32, 8, 2136, 8, 11, 23424, 1};

public:
    explicit Inference(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("Missing TensorRT plan: " + path.string());
        const auto size = input.tellg();
        if (size <= 0) throw std::runtime_error("Empty TensorRT plan");
        std::pmr::vector<char> bytes(static_cast<std::size_t>(size));
        input.seekg(0);
        input.read(bytes.data(), bytes.size());
        runtime.reset(interop::trt::createInferRuntime(logger));
        if (!runtime) throw std::runtime_error("Cannot create TensorRT runtime");
        engine.reset(runtime->deserializeCudaEngine(bytes.data(), bytes.size()));
        if (!engine) throw std::runtime_error("Cannot deserialize plan; rebuild for this TensorRT/GPU");
        context.reset(engine->createExecutionContext());
        if (!context || engine->getNbIOTensors() != 9) throw std::runtime_error("Unexpected MotionBricks engine contract");
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto  shape = engine->getTensorShape(names[i]);
            std::size_t count = 1;
            for (int d = 0; d < shape.nbDims; ++d) count *= shape.d[d];
            const bool integral = i == 1 || i == 3 || i == 5 || i == 6 || i == 8;
            if (count != sizes[i] || engine->getTensorDataType(names[i]) != (integral ? interop::trt::DataType::kINT32 : interop::trt::DataType::kFLOAT))
                throw std::runtime_error("Wrong tensor shape/type");
            Cuda(interop::cudaMalloc(&buffers[i].data, sizes[i] * 4));
            if (!context->setTensorAddress(names[i], buffers[i].data)) throw std::runtime_error("Cannot bind model tensor");
        }
    }
    ~Inference() { interop::cudaStreamSynchronize(stream.value); }
    void Run(const std::array<float, 40>& roots, const std::array<float, 32>& velocities,
             const std::array<float, 2136>& poses, std::array<float, 23424>& result, int& tokens,
             const std::array<int,8>& present, const std::array<int,8>& velocities_present, const std::array<int,11>& allowed) {
        // Masks and duration choices come from the active preset target.
        std::array<const void*, 7> host{roots.data(), present.data(), velocities.data(), velocities_present.data(), poses.data(), present.data(), allowed.data()};
        for (std::size_t i = 0; i < 7; ++i) Cuda(interop::cudaMemcpyAsync(buffers[i].data, host[i], sizes[i] * 4, interop::cudaMemcpyHostToDevice, stream.value));
        if (!context->enqueueV3(stream.value)) throw std::runtime_error("MotionBricks enqueue failed");
        Cuda(interop::cudaMemcpyAsync(result.data(), buffers[7].data, sizes[7] * 4, interop::cudaMemcpyDeviceToHost, stream.value));
        Cuda(interop::cudaMemcpyAsync(&tokens, buffers[8].data, 4, interop::cudaMemcpyDeviceToHost, stream.value));
        Cuda(interop::cudaStreamSynchronize(stream.value));
        if (tokens < 6 || tokens > 16 || !allowed[tokens-6] || !std::all_of(result.begin(), result.begin() + tokens * 4 * 366,
            [](float v) { return std::isfinite(v); })) throw std::runtime_error("Invalid prediction");
    }
};

struct SourceJoint {
    int   parent = -1, model = -1;
    Joint rest;
};
struct TargetJoint {
    std::pmr::string name;
    int              parent = -1, driver = -1, source = -1;
    Joint            rest, bind;
    Quat             correction;
    ecs::Entity      entity;
};
struct Foot {
    int                  hip = 0, knee = 0, ankle = 0;
    Vec                  heel, toe;
    std::array<Vec, 2>   anchors{};
    std::array<bool, 2>  planted{};
    std::array<float, 2> weights{};
};
struct Frame {
    SomaPose             pose;
    std::array<float, 4> contacts{};
};
struct PresetTarget {
    std::string name;
    int count = 4, early = 0, minimum_tokens = 6, maximum_tokens = 14, pins = 0;
    std::array<SomaPose, 4> poses;
};
struct PresetSequence { std::string name; std::vector<int> targets; };
struct Preset { std::string name; std::vector<PresetTarget> targets; std::vector<PresetSequence> sequences; };
struct GaitCycle { int period=0; float speed=0; int origin=0; std::vector<SomaPose> poses; };
struct ActionClip {
    std::string name;
    int core_end=0, active_start=-1, active_end=-1, air_start=-1, air_end=-1;
    std::vector<Frame> frames;
};
struct Stats {
    unsigned inferences = 0, steps = 0;
    double   inference_ms = 0;
    Vec      root{};
    Vec      display_root{};
    float    minimum_sole = 0, maximum_root_step = 0;
    float    maximum_double_support_gap = 0;
    unsigned double_support_samples     = 0;
    float    minimum_knee_forward = 1;
    unsigned knee_samples = 0;
    float maximum_pelvis_drop = 0;
    float maximum_pelvis_compression = 0;
    float pelvis_drop = 0;
    float minimum_locomotion_condition_speed = std::numeric_limits<float>::max();
    int predicted_tokens = 0, minimum_tokens = 16, maximum_tokens = 0;
    unsigned action_targets=0, completed_actions=0, pinned_frames=0;
    std::array<unsigned,4> ability_started{}, ability_completed{};
    unsigned strike_events=0, ability_recoveries=0, ability_model_targets=0, ability_model_frames=0, airborne_render_frames=0;
};

class Controller {
public:
    void EnableInteractions(bool enabled) { environment_enabled=enabled; }
    void PreserveTargetGroup(bool enabled) { preserve_target_group = enabled; }
    auto ForwardWalkSpeed() const -> float { return forward_cycles[0].speed; }
    void ConfigureGaitProbe(std::string options, const std::filesystem::path& directory) {
        gait_probe = std::move(options);
        gait_plans.open(directory / "plans.csv");
        gait_plans << "plan,frame,stage,side,root_x,root_y,root_z,foot_x,foot_y,foot_z,contact\n" << std::setprecision(9);
        if(std::filesystem::exists(directory / "target.txt")) {
            std::ifstream input(directory / "target.txt");
            for(auto& p:presets[0].targets[1].poses) for(auto& j:p) input>>j.position.x>>j.position.y>>j.position.z>>j.rotation.x>>j.rotation.y>>j.rotation.z>>j.rotation.w;
            if(!input) throw std::runtime_error("Invalid gait probe target");
        }
        if(std::filesystem::exists(directory / "cycle.txt")) {
            std::ifstream input(directory / "cycle.txt");
            gait_cycle.resize(43);
            for(auto& p:gait_cycle) for(auto& j:p) input>>j.position.x>>j.position.y>>j.position.z>>j.rotation.x>>j.rotation.y>>j.rotation.z>>j.rotation.w;
            if(!input) throw std::runtime_error("Invalid gait probe cycle");
        }
    }
    Controller(const std::filesystem::path& data, const std::filesystem::path& plan) : inference(plan) {
        std::ifstream input(data);
        std::string   magic;
        std::size_t   ns = 0, nt = 0, nc = 0;
        input >> magic >> ns >> nt >> nc >> scale;
        if (magic != "HMB1" || ns != 77 || nt > 1024 || nt == 0 || nc != 3) throw std::runtime_error("Invalid runtime calibration");
        auto read_pose = [&](Joint& p) {
            input >> p.position.x >> p.position.y >> p.position.z >> p.rotation.x >> p.rotation.y >> p.rotation.z >> p.rotation.w;
            if (!input || !std::ranges::all_of(p.position.data, [](float v) { return std::isfinite(v); }) ||
                !std::isfinite(p.rotation.norm()) || std::abs(p.rotation.norm() - 1) > .001f)
                throw std::runtime_error("Invalid calibration pose");
        };
        model_to_source.fill(-1);
        for (int i = 0; i < 77; ++i) {
            auto& s = source[i];
            input >> s.parent >> s.model;
            read_pose(s.rest);
            if (s.parent >= i || s.parent < -1 || (i > 0 && s.parent < 0) || s.model < -1 || s.model >= 30) throw std::runtime_error("Invalid SOMA layout");
            if (s.model >= 0) {
                if (model_to_source[s.model] >= 0) throw std::runtime_error("Duplicate model joint");
                model_to_source[s.model] = i;
            }
        }
        if (std::ranges::find(model_to_source, -1) != model_to_source.end()) throw std::runtime_error("Missing model joint");
        target.resize(nt);
        local.resize(nt);
        world.resize(nt);
        for (auto& t : target) {
            std::string name;
            input >> std::quoted(name) >> t.parent >> t.driver >> t.source;
            t.name = name;
            read_pose(t.rest);
            read_pose(t.bind);
            input >> t.correction.x >> t.correction.y >> t.correction.z >> t.correction.w;
        }
        for (auto& f : feet) input >> f.hip >> f.knee >> f.ankle >> f.heel.x >> f.heel.y >> f.heel.z >> f.toe.x >> f.toe.y >> f.toe.z;
        for (std::size_t i = 0; i < target.size(); ++i) {
            const auto& t = target[i];
            if (t.parent < -1 || t.parent >= static_cast<int>(i) || t.driver < -1 || t.driver >= static_cast<int>(i) || t.source < -1 || t.source >= 77)
                throw std::runtime_error("Invalid target layout/driver order");
        }
        for (const auto& f : feet)
            if (f.hip < 0 || f.knee < 0 || f.ankle < 0 || f.ankle >= nt || f.knee >= nt || f.hip >= nt)
                throw std::runtime_error("Invalid foot chain");
        if (!std::isfinite(scale) || scale <= 0) throw std::runtime_error("Invalid retarget scale");
        for (auto& bank : banks) {
            std::size_t count = 0;
            input >> count;
            if (count < 4 || count > 100000) throw std::runtime_error("Invalid conditioning bank size");
            bank.resize(count);
            for (auto& p : bank)
                for (auto& j : p) read_pose(j);
        }
        if (!input) throw std::runtime_error("Truncated runtime calibration");
        std::ifstream preset_input(data.parent_path() / "presets.txt");
        int preset_count = 0;
        preset_input >> magic >> preset_count;
        if (magic != "HMP1" || preset_count != 8) throw std::runtime_error("Missing/invalid official presets; run prepare_presets.py");
        const auto read_preset_pose = [&](Joint& p) {
            preset_input >> p.position.x >> p.position.y >> p.position.z >> p.rotation.x >> p.rotation.y >> p.rotation.z >> p.rotation.w;
            if (!preset_input || !std::isfinite(p.position.norm()) || std::abs(p.rotation.norm()-1) > .001f)
                throw std::runtime_error("Invalid preset pose");
        };
        for (auto& preset : presets) {
            int nt = 0, ns = 0;
            preset_input >> std::quoted(preset.name) >> nt >> ns;
            if (nt < 1 || nt > 100 || ns < 0 || ns > 100) throw std::runtime_error("Invalid preset counts");
            preset.targets.resize(nt); preset.sequences.resize(ns);
            for (auto& t : preset.targets) {
                preset_input >> std::quoted(t.name) >> t.count >> t.early >> t.minimum_tokens >> t.maximum_tokens >> t.pins;
                if (t.count<1 || t.count>4 || t.early<0 || t.minimum_tokens<6 || t.maximum_tokens>16 || t.minimum_tokens>t.maximum_tokens || t.pins>15)
                    throw std::runtime_error("Invalid target inference parameters");
                for (auto& p : t.poses) for (auto& j : p) read_preset_pose(j);
            }
            for (auto& sequence : preset.sequences) {
                int count=0; preset_input >> std::quoted(sequence.name) >> count;
                if(count<1 || count>100) throw std::runtime_error("Invalid sequence size");
                sequence.targets.resize(count);
                for(auto& index:sequence.targets) { preset_input>>index; if(index<0 || index>=nt) throw std::runtime_error("Invalid target index"); }
            }
        }
        int loop_count=0; preset_input>>loop_count;
        if(loop_count!=3) throw std::runtime_error("Invalid idle loop count");
        for(auto& loop:idle_loops) {
            int count=0; preset_input>>count;
            if(count<4 || count>100000) throw std::runtime_error("Invalid idle cycle");
            loop.resize(count);
            for(auto& p:loop) for(auto& j:p) read_preset_pose(j);
        }
        std::ifstream cycle_input(data.parent_path()/"gait_cycles.txt");
        int cycle_count=0; cycle_input>>magic>>cycle_count;
        if(magic!="HMG2" || cycle_count!=9) throw std::runtime_error("Prepare gait cycles with prepare_gait_cycles.py");
        for(auto& cycle:forward_cycles) {
            cycle_input>>cycle.period>>cycle.speed>>cycle.origin;
            if(cycle.period<8 || cycle.period>120 || cycle.origin<0 || cycle.origin>=cycle.period || !std::isfinite(cycle.speed) || cycle.speed<.05f) throw std::runtime_error("Invalid gait cycle");
            cycle.poses.resize(cycle.period+8);
            for(auto& pose:cycle.poses) for(auto& j:pose) {
                cycle_input>>j.position.x>>j.position.y>>j.position.z>>j.rotation.x>>j.rotation.y>>j.rotation.z>>j.rotation.w;
                if(!cycle_input || !std::isfinite(j.position.norm()) || !std::isfinite(j.rotation.norm()) || std::abs(j.rotation.norm()-1)>.001f) throw std::runtime_error("Invalid gait pose");
            }
        }
        std::ifstream actions(data.parent_path()/"actions.txt");
        int action_count=0; actions>>magic>>action_count;
        if(magic!="HMA1" || action_count!=5) throw std::runtime_error("Prepare action library with prepare_actions.py");
        for(auto& clip:action_clips) {
            int count=0;
            actions>>std::quoted(clip.name)>>count>>clip.core_end>>clip.active_start>>clip.active_end>>clip.air_start>>clip.air_end;
            if(count<8 || count>900 || clip.core_end<4 || clip.core_end>count-4) throw std::runtime_error("Invalid action clip timing");
            clip.frames.resize(count);
            for(auto& frame:clip.frames) {
                for(auto& contact:frame.contacts) actions>>contact;
                for(auto& j:frame.pose) {
                    actions>>j.position.x>>j.position.y>>j.position.z>>j.rotation.x>>j.rotation.y>>j.rotation.z>>j.rotation.w;
                    if(!actions || !std::isfinite(j.position.norm()) || !std::isfinite(j.rotation.norm()) || std::abs(j.rotation.norm()-1)>.001f) throw std::runtime_error("Invalid action pose");
                }
                frame.pose=ToLocal(frame.pose);
            }
        }
        Reset();
    }
    void Reset() {
        ability=-1; combo_index=0; combo_queued=false; ability_frame=0; ability_recovery=false; ability_hit=false;
        current.contacts = {};
        const auto& reset_pose=idle_loops[selected_preset==2 ? 1 : 0][0];
        current.pose     = ToLocal(reset_pose);
        Joint align{.position = Vec{current.pose[0].position.x, 0, current.pose[0].position.z},
                    .rotation = Yaw(Heading(reset_pose))};
        current.pose[0] = Relative(align, current.pose[0]);
        previous        = current;
        history.fill(current.pose);
        generated.clear();
        diagnostic_guides = {}; diagnostic_display = {}; diagnostic_raw_roots.clear();
        diagnostic_roots = {}; diagnostic_velocities = {}; diagnostic_poses = {};
        segment         = 0;
        accumulator     = 0;
        last_request    = 100;
        desired_heading = 0;
        last_mode       = -1;
        last_direction  = {};
        for (auto& f : feet) {
            f.planted = {};
            f.weights = {};
        }
        stats = {};
        phase_cycle=-1; diagnostic_phase_cycle=-1; phase_position=0; phase_step=0;
        forward_speed=0; phase_clock=0; previous_phase_clock=0;
        action = false; seated = false; looping = false; action_step = 0;
        StartLoop(selected_preset==2 ? 1 : 0);
    }
    void SelectPreset(int index) {
        if(index<0 || index>=8) throw std::invalid_argument("Invalid preset selection");
        if(action || seated || ability>=0) return;
        // Interaction selection is an optional variant preference, never a teleport.
        if(index>=4) { preferred_interaction=index; return; }
        selected_preset=index; variant=0;
        looping=false; generated.clear(); last_request=100;
    }
    void NextVariant() {
        if(action || seated || ability>=0) return;
        const int nearby=NearbyInteraction();
        if(nearby>=4) {
            interaction_variants[nearby-4]=(interaction_variants[nearby-4]+1)%int(presets[nearby].sequences.size());
            if(nearby==7) interaction_variants[3]=0;
            return;
        }
        const int count=selected_preset==0 ? 3 : selected_preset==3 ? 6 : 1;
        variant=(variant+1)%count;
        generated.clear(); last_request=100;
    }
    void Interact() {
        if(action || ability>=0) return;
        if(seated) sequence_index=1;
        else {
            const int nearby=NearbyInteraction();
            if(nearby<0) return;
            action_preset=nearby;
            sequence_index=interaction_variants[nearby-4];
            action_placement=Joint{.position=ObjectOffset(nearby),.rotation=Quat::identity()};
        }
        free_jump=false;
        seated=false; looping=false; action=true; action_step=0; generated.clear();
    }
    // The scene's authored affordances share the same placement as its boxes.
    // Gate by approach side, lateral clearance, distance and actual body heading.
    auto Approach(int preset) const -> Joint {
        const int v=interaction_variants[preset-4];
        const auto& entry=presets[preset].targets[presets[preset].sequences[v].targets[0]].poses[0];
        const float h=Heading(entry);
        Vec p=entry[0].position+ObjectOffset(preset)-Vec{std::sin(h),0,std::cos(h)}*.65f;
        p.y=0;
        return {.position=p,.rotation=Yaw(h)};
    }
    auto NearbyInteraction() const -> int {
        if(!environment_enabled || action || seated || ability>=0) return -1;
        const auto pose=ToWorld(current.pose);
        float best=1e9f; int result=-1;
        for(int p=4;p<8;++p) {
            const auto approach=Approach(p);
            Vec root=pose[0].position; root.y=0;
            const Vec relative=Rotate(Inverse(approach.rotation),root-approach.position);
            const float facing=math::dot(Rotate(approach.rotation,Vec{0,0,1}),Vec{std::sin(Heading(pose)),0,std::cos(Heading(pose))});
            if(std::abs(relative.x)>.40f || relative.z<-.85f || relative.z>.25f || facing<.65f) continue;
            if(relative.norm()<best) {best=relative.norm();result=p;}
        }
        return result;
    }
    auto InteractionPrompt() const -> std::string {
        if(ability==0) return std::format("COMBO {}/3: {} / {} | {}",combo_index+1,action_clips[AbilityClipIndex()].name,AbilityStage(),combo_queued ? "NEXT HIT QUEUED" : ability_recovery ? "recovering" : "F: queue next hit");
        if(ability>=0) return std::format("{} / {} | frame {} | F attack | J jump | Ctrl roll",action_clips[AbilityClipIndex()].name,AbilityStage(),ability_frame);
        if(!environment_enabled) return "F: punch | J: forward jump | Air + F: jump attack | Ctrl: forward roll";
        if(seated) return "SPACE: stand up";
        if(action) return free_jump ? "JUMP" : "INTERACTING";
        const int p=NearbyInteraction();
        if(p<0) {
            float best=1e9f; int nearest=4;
            for(int i=4;i<8;++i) {
                Vec d=Approach(i).position-current.pose[0].position;d.y=0;
                if(d.norm()<best) {best=d.norm();nearest=i;}
            }
            return std::format("{}: {:.1f} m to approach arrow | Face along arrow | J: jump",presets[nearest].sequences[interaction_variants[nearest-4]].name,best*scale);
        }
        return "SPACE: "+presets[p].sequences[interaction_variants[p-4]].name+" | Tab: variant | J: jump";
    }
    void SetupInteractionTest(int preset, int v) {
        interaction_variants[preset-4]=v;
        const auto approach=Approach(preset);
        const auto place=[&](float distance,float turn) {
            Reset();
            Joint placement=approach;
            placement.position-=Rotate(approach.rotation,Vec{0,0,distance});
            placement.rotation=approach.rotation*Yaw(turn);
            current.pose[0]=Compose(placement,current.pose[0]);
            previous=current; history.fill(current.pose);
            desired_heading=Heading(ToWorld(current.pose)); StartLoop(0);
        };
        place(0,std::numbers::pi_v<float>);
        if(NearbyInteraction()==preset) throw std::runtime_error("Interaction allowed while facing away");
        place(0,0);
        if(NearbyInteraction()!=preset) throw std::runtime_error("Interaction approach was not recognized");
        const Vec before=current.pose[0].position;
        SelectPreset(preset);
        if((before-current.pose[0].position).norm()>1e-6f) throw std::runtime_error("Preset selection teleported character");
        place(2.0f,0);
        Interact();
        if(action) throw std::runtime_error("Remote interaction was allowed");
    }
    auto InteractionTestDirection(int preset) const -> Vec {
        if(action || seated || stats.completed_actions) return {};
        Vec delta=Approach(preset).position-current.pose[0].position;delta.y=0;
        return delta.norm()>.1f ? math::normalize(delta) : Vec{};
    }
    auto IsActing() const -> bool { return action || ability>=0; }
    void Jump() { RequestAbility(1); }
    void RequestAbility(int kind, Vec direction={}) {
        if(kind<0 || kind>3 || action || seated) return;
        if(ability>=0) {
            if(kind==0 && ability==0 && combo_index<2 && ability_stage<=2) { combo_queued=true; return; }
            if(kind==0 && ability==1 && !ability_recovery && ability_frame<=20) {
                ability=2; generated.clear(); ability_phase_start=ability_frame;
                ++stats.ability_started[2];
            }
            return;
        }
        combo_index=0; combo_queued=false; ability=kind; ability_frame=0; ability_stage=0; ability_phase_start=0; ability_recovery=false; ability_hit=false;
        const float facing=direction.norm()>.01f ? std::atan2(direction.x,direction.z) : Heading(ToWorld(current.pose));
        ability_placement={.position={current.pose[0].position.x,0,current.pose[0].position.z},.rotation=Yaw(facing)};
        desired_heading=facing; looping=false; generated.clear(); last_mode=-1;
        ++stats.ability_started[kind];
    }
    auto AbilityStage() const -> std::string_view {
        if(ability<0) return "locomotion";
        if(ability_recovery) return "model recovery";
        if(ability==0) return std::array<std::string_view,4>{"prepare","punch","follow-through","recovery"}[ability_stage];
        const auto& clip=action_clips[AbilityClipIndex()];
        if(ability_frame>=clip.active_start && ability_frame<=clip.active_end) return "strike";
        if(AbilityAirborne()) return ability==3 ? "roll" : "airborne";
        return ability_frame<6 ? "prepare" : "settle";
    }
    void WriteAbilityDiagnostics(std::ostream& out, unsigned frame) const {
        out<<frame<<','<<ability<<','<<ability_frame<<','<<AbilityStage()<<','<<stats.root.x<<','<<stats.root.y<<','<<stats.root.z<<','<<stats.strike_events<<','<<stats.ability_recoveries<<','<<combo_index<<','<<combo_queued;
        const auto pose=ToWorld(current.pose);
        for(int j:{13,19}) {const auto p=Rotate(Yaw(-desired_heading),pose[model_to_source[j]].position-pose[0].position);out<<','<<p.x<<','<<p.y<<','<<p.z;}
        out<<'\n';
    }
    auto IsSeated() const -> bool { return seated; }
    auto Facing() const -> float { return Heading(ToWorld(current.pose)); }
    template<class DrawLine>
    void DrawInteractions(DrawLine line) const {
        if(!environment_enabled) return;
        const int nearby=NearbyInteraction();
        for(int i=4;i<8;++i) {
            const auto approach=Approach(i);
            const Vec p=approach.position*scale+Vec{0,.02f,0};
            const Vec forward=Rotate(approach.rotation,Vec{0,0,.45f});
            const Vec side=Rotate(approach.rotation,Vec{.16f,0,0});
            const auto color=i==nearby ? math::Color(.3f,1,.4f,1) : i==preferred_interaction ? math::Color(1,.8f,.2f,1) : math::Color(.7f,.75f,.8f,1);
            line(p-forward,p+forward,color);
            line(p+forward,p+forward*.4f+side,color);
            line(p+forward,p+forward*.4f-side,color);
            line(p-side,p+side,color);
        }
    }
    auto PresetLabel() const -> std::string {
        static constexpr std::array labels{"1 Forward", "2 Strafing", "3 Crouch", "4 Styles", "5 Avoidance", "6 Jump 0.5m", "7 Jump 0.75m", "8 Sit"};
        const auto& p=presets[selected_preset];
        const std::string variant_name=selected_preset==0 ? std::array<std::string,3>{"Walk","Jog","Run"}[variant] : selected_preset==3 ? p.targets[variant+1].name : selected_preset>=4 ? p.sequences[seated ? 0 : variant].name : "WASD strafe; Q/E turn";
        return std::string(labels[selected_preset])+" / "+variant_name;
    }
    void Turn(float angle) {
        if(!action && !seated && ability<0 && std::abs(angle)>1e-6f) {desired_heading+=angle;turn_requested=true;}
    }
    void Update(asset::Scene& scene, double dt, Vec direction, bool fast) {
        if (bound != &scene) Bind(scene);
        if (!std::isfinite(dt) || dt < 0 || !std::isfinite(direction.norm())) throw std::invalid_argument("Invalid runtime input");
        direction.y = 0;
        if (direction.norm() > 1) direction = math::normalize(direction);
        accumulator += std::min(dt, .1);
        while (accumulator >= 1.0 / 30.0) {
            accumulator -= 1.0 / 30.0;
            if(ability>=0) {
                previous=current;
                AdvanceAbility();
                for(int i=0;i<3;++i) history[i]=history[i+1];
                history[3]=current.pose;
                ++stats.steps;
                stats.maximum_root_step=std::max(stats.maximum_root_step,(current.pose[0].position-previous.pose[0].position).norm());
                continue;
            }
            const int mode = direction.norm() < .01f || action || seated ? 0 : (fast ? 2 : 1);
            const bool phase_control=preserve_target_group && (gait_probe.empty() || gait_probe.contains("phasefix"));
            speed_transition=false;
            if(mode && selected_preset==0 && phase_control) {
                const float requested=forward_cycles[mode==2 ? 2 : variant].speed;
                if(forward_speed<=0) forward_speed=forward_cycles[0].speed;
                forward_speed+=std::clamp(requested-forward_speed,-5.0f/30,4.0f/30);
                speed_transition=std::abs(requested-forward_speed)>1e-4f;
                const auto [low,blend]=ForwardBlend();
                effective_period=forward_cycles[low].period*(1-blend)+forward_cycles[low+1].period*blend;
            } else {
                forward_speed=0;
                effective_period=selected_preset==3 && mode ? float(forward_cycles[3+variant].period) : 30.0f;
            }
            if(mode) phase_clock+=1.0/effective_period;
            if (mode && selected_preset!=1 && selected_preset!=2) desired_heading = std::atan2(direction.x, direction.z);
            const bool changed = mode != last_mode || (direction - last_direction).norm() > .1f || (turn_requested && last_request>=3);
            if (looping && mode==0 && !turn_requested) {
                previous=current; SampleLoop();
            } else {
            if(looping) { looping=false; generated.clear(); }
            const bool end = !generated.empty() && segment>=generated.size()-1-std::min<std::size_t>(active_early,generated.size()-2);
            if(action && end) {
                if(++action_step>=presets[action_preset].sequences[sequence_index].targets.size()) {
                    ++stats.completed_actions;
                    action=false;
                    if(action_preset==7 && sequence_index==0) { seated=true; StartLoop(2); }
                    else { looping=false; last_mode=-1; }
                }
                generated.clear();
            }
            if(!looping && (generated.empty() || (action ? end : mode ? segment>=(speed_transition ? 3 : gait_probe.contains("replan16") ? 16 : 8) || (changed && last_request>=3) : end || changed))) {
                Generate(mode, desired_heading, direction);
                last_direction = direction;
                last_mode      = mode;
                last_request   = 0;
            }
            previous = current;
            if(looping) SampleLoop();
            else {
                current = generated[std::min(++segment,generated.size()-1)];
                if(!action && mode==0 && !turn_requested && segment>=generated.size()-1) StartLoop(selected_preset==2 ? 1 : 0);
            }
            }
            for (int i = 0; i < 3; ++i) history[i] = history[i + 1];
            history[3] = current.pose;
            ++last_request;
            ++stats.steps;
            const float step        = (current.pose[0].position - previous.pose[0].position).norm();
            stats.maximum_root_step = std::max(stats.maximum_root_step, step);
        }
        Frame       display = current;
        const float alpha   = static_cast<float>(accumulator * 30);
        for (int i = 0; i < 77; ++i) display.pose[i] = asset::BlendLocalPose(previous.pose[i], current.pose[i], alpha);
        for (int i = 0; i < 4; ++i) display.contacts[i] = previous.contacts[i] * (1 - alpha) + current.contacts[i] * alpha;
        Retarget(display, static_cast<float>(std::min(dt, .1)));
        stats.root = current.pose[0].position;
        turn_requested=false;
    }
    auto GetStats() const -> const Stats& { return stats; }
    auto IsLooping() const -> bool { return looping; }
    auto LoopPeriod() const -> std::size_t { return idle_loops[loop_index].size(); }
    auto RenderedStabilityPose() const -> std::array<Joint,5> {
        std::array<Joint,5> result{};
        const std::array model_indices{0,13,19,24,28};
        for(std::size_t i=0;i<5;++i) for(const auto& t:target)
            if(t.source==model_to_source[model_indices[i]]) {
                const auto& m=t.entity.Get<asset::Transform>().world_matrix;
                result[i]={.position=math::get_translation(m),.rotation=math::get_rotation(m)}; break;
            }
        return result;
    }
    void WriteModelDiagnostics(std::ostream& out) const {
        const auto floats = [&](const auto& values) {
            out << '['; bool first = true;
            for (auto v : values) { if (!first) out << ','; first = false; out << v; }
            out << ']';
        };
        out << "{\"inference\":" << stats.inferences << ",\"roots\":"; floats(diagnostic_roots);
        out << ",\"velocities\":"; floats(diagnostic_velocities);
        out << ",\"poses\":"; floats(diagnostic_poses);
        out << ",\"preset\":" << selected_preset << ",\"variant\":" << variant << ",\"action\":" << action;
        out << ",\"ability\":" << ability << ",\"reference_clip\":" << (ability<0 ? -1 : AbilityClipIndex()) << ",\"ability_stage\":" << ability_stage
            << ",\"reference_first_frame\":" << (ability<0 ? -1 : AbilityWindows().at(ability_stage));
        out << ",\"phase_cycle\":" << diagnostic_phase_cycle << ",\"phase\":" << phase_position
            << ",\"phase_clock\":" << phase_clock << ",\"speed_blend\":" << diagnostic_speed_blend
            << ",\"phase_period\":" << (diagnostic_phase_cycle<0 ? 0 : forward_cycles[diagnostic_phase_cycle].period)
            << ",\"phase_adjustment\":" << diagnostic_phase_adjustment << ",\"motion_step\":" << stats.steps
            << ",\"command_velocity_world\":"; floats(diagnostic_command_velocity.data);
        out << ",\"root_mask\":"; floats(diagnostic_mask);
        out << ",\"pose_mask\":"; floats(diagnostic_mask);
        out << ",\"velocity_mask\":"; floats(diagnostic_velocity_mask);
        out << ",\"allowed_token_mask\":"; floats(diagnostic_allowed);
        out << ",\"predicted_tokens\":" << stats.predicted_tokens
            << ",\"target_time_seconds\":" << (generated.empty() ? 0 : float(generated.size()-1)/30);
        out << ",\"guide_world_model_joints\":[";
        bool first = true;
        for (const auto& pose : diagnostic_guides) for (int j : model_to_source) {
            if (!first) out << ','; first = false;
            const auto& p=pose[j]; floats(std::array{p.position.x,p.position.y,p.position.z,p.rotation.x,p.rotation.y,p.rotation.z,p.rotation.w});
        }
        out << "],\"raw_root_world\":[";
        first=true;
        for (auto p : diagnostic_raw_roots) { if(!first) out<<','; first=false; floats(std::array{p.x,p.y,p.z}); }
        out << "],\"corrected_root_world\":[";
        first=true;
        for (const auto& f : generated) { if(!first) out<<','; first=false; const auto p=f.pose[0].position; floats(std::array{p.x,p.y,p.z}); }
        out << "]}\n";
    }
    template<class DrawLine>
    void DrawDiagnostics(DrawLine line, bool skeletons) const {
        if(skeletons) for(const auto& foot:feet) {
            const auto at=[&](int j) { return math::get_translation(target[j].entity.Get<asset::Transform>().world_matrix); };
            line(at(foot.hip),at(foot.knee),math::Color(1,.45f,.12f,1));
            line(at(foot.knee),at(foot.ankle),math::Color(1,.45f,.12f,1));
            const auto& matrix=target[foot.ankle].entity.Get<asset::Transform>().world_matrix;
            const auto rotation=math::get_rotation(matrix);
            line(at(foot.ankle)+Rotate(rotation,foot.heel),at(foot.ankle)+Rotate(rotation,foot.toe),math::Color(1,.45f,.12f,1));
        }
        Vec offset{};
        for (const auto& t : target) if (t.source == 0) { offset = t.bind.position - Vec{0, source[0].rest.position.y * scale, 0}; break; }
        const auto point = [&](Vec p) { return p * scale + offset; };
        const auto skeleton = [&](const SomaPose& pose, math::Color color) {
            // Only the 30 model joints, connected to the nearest model ancestor.
            for (int j : model_to_source) {
                int parent = source[j].parent;
                while (parent >= 0 && source[parent].model < 0) parent = source[parent].parent;
                if (parent >= 0) line(point(pose[parent].position), point(pose[j].position), color);
            }
        };
        if(looping || (!stats.inferences && ability<0)) {
            if(skeletons) skeleton(diagnostic_display,math::Color(.25f,.8f,1,1));
            return;
        }
        for (int f = 1; f < 4; ++f) line(point(diagnostic_guides[f-1][0].position), point(diagnostic_guides[f][0].position), math::Color(.65f,.7f,.8f,1));
        // The dashed connector is a visualization of endpoint intent, not an
        // additional dense trajectory input to this model.
        const Vec from = point(diagnostic_guides[3][0].position), to = point(diagnostic_guides[4][0].position);
        for (int i=0;i<12;i+=2) line(from+(to-from)*(float(i)/12),from+(to-from)*(float(i+1)/12),math::Color(1,.8f,.2f,1));
        for (int f = 4; f < 8 && diagnostic_mask[f]; ++f) {
            const auto p=point(diagnostic_guides[f][0].position);
            line(p-Vec{.025f,0,0},p+Vec{.025f,0,0},math::Color(1,.8f,.2f,1));
            line(p-Vec{0,.025f,0},p+Vec{0,.025f,0},math::Color(1,.8f,.2f,1));
            if(f>4) line(point(diagnostic_guides[f-1][0].position),p,math::Color(1,.8f,.2f,1));
            if(skeletons) skeleton(diagnostic_guides[f],math::Color(1,.8f,.2f,f==4?.9f:.25f));
        }
        for (std::size_t f=1;f<generated.size();++f) {
            line(point(diagnostic_raw_roots[f-1]),point(diagnostic_raw_roots[f]),math::Color(1,.3f,.7f,1));
            line(point(generated[f-1].pose[0].position),point(generated[f].pose[0].position),math::Color(.2f,1,.55f,1));
        }
        if(skeletons) skeleton(diagnostic_display,math::Color(.25f,.8f,1,1));
        const auto p=point(diagnostic_display[0].position);
        line(p,stats.display_root,math::Color(1,.35f,.15f,1));
    }
    void WriteRenderedJoints(std::ostream& output, unsigned frame) const {
        for (std::size_t i = 0; i < target.size(); ++i) {
            const auto& matrix = target[i].entity.Get<asset::Transform>().world_matrix;
            const auto p = math::get_translation(matrix);
            const auto q = math::get_rotation(matrix);
            output << frame << ',' << target[i].name << ',' << p.x << ',' << p.y << ',' << p.z << ',' << q.x << ',' << q.y << ',' << q.z << ',' << q.w << '\n';
        }
    }
    void WriteIdleDiagnostics(std::ostream& out, unsigned frame) const {
        for(int side=0;side<2;++side) {
            out << frame << ',' << looping << ',' << loop_frame << ',' << side;
            const auto vec=[&](Vec p){ out << ',' << p.x << ',' << p.y << ',' << p.z; };
            vec(diagnostic_display[0].position);
            vec(diagnostic_display[model_to_source[side==0 ? 23 : 27]].position);
            vec(diagnostic_display[model_to_source[side==0 ? 24 : 28]].position);
            for(int index : {feet[side].hip,feet[side].knee,feet[side].ankle})
                vec(math::get_translation(target[index].entity.Get<asset::Transform>().world_matrix));
            for(auto p:feet[side].anchors) vec(p);
            out << ',' << feet[side].weights[0] << ',' << feet[side].weights[1] << '\n';
        }
    }
    void WriteGaitDiagnostics(std::ostream& output, unsigned frame) const {
        const auto rendered = RenderedStabilityPose();
        for(int side=0;side<2;++side) {
            const auto root=diagnostic_display[0].position;
            const auto foot=diagnostic_display[model_to_source[side==0 ? 24 : 28]].position;
            const auto rr=rendered[0].position, rf=rendered[3+side].position;
            output << frame << ',' << side << ',' << root.x << ',' << root.y << ',' << root.z
                   << ',' << foot.x << ',' << foot.y << ',' << foot.z
                   << ',' << rr.x << ',' << rr.y << ',' << rr.z
                   << ',' << rf.x << ',' << rf.y << ',' << rf.z << '\n';
        }
    }
    auto RenderedHead() const -> Vec {
        for (const auto& joint : target)
            if (joint.source == model_to_source[6] && joint.entity.Valid()) return math::get_translation(joint.entity.Get<asset::Transform>().world_matrix);
        throw std::runtime_error("No bound head");
    }
    auto RenderedRoot() const -> Vec {
        for (const auto& joint : target)
            if (joint.source == 0 && joint.entity.Valid()) return math::get_translation(joint.entity.Get<asset::Transform>().world_matrix);
        throw std::runtime_error("No bound pelvis");
    }

private:
    bool environment_enabled=true;
    auto AbilityClipIndex() const -> int { return ability==0 && combo_index==1 ? 4 : ability; }
    auto AbilityAirborne() const -> bool {
        if(ability<0 || ability_recovery) return false;
        const auto& clip=action_clips[AbilityClipIndex()];
        const int sampled_frame=ability_frame;
        return clip.air_start>=0 && sampled_frame>=clip.air_start && sampled_frame<=clip.air_end;
    }
    auto PlacedAbilityFrame(int f) const -> Frame {
        auto frame=action_clips[AbilityClipIndex()].frames[std::clamp(f,0,int(action_clips[AbilityClipIndex()].frames.size())-1)];
        frame.pose[0]=Compose(ability_placement,frame.pose[0]);
        return frame;
    }
    auto AbilityWindows() const -> std::vector<int> {
        // Semantic endpoints; each endpoint supplies four CONSECUTIVE 30 Hz poses.
        // The model fills every interval, including preparation and the action core.
        switch(ability) {
        case 0: return {4,8,18,26};
        case 1: return {12,22,34,44};
        case 2: return {12,20,34,44};
        default: return {12,24,36,56};
        }
    }
    auto AbilityTarget() const -> PresetTarget {
        PresetTarget t;
        t.count=4; t.minimum_tokens=6; t.maximum_tokens=6;
        const int first=AbilityWindows().at(ability_stage);
        for(int f=0;f<4;++f) t.poses[f]=ToWorld(PlacedAbilityFrame(first+f).pose);
        return t;
    }
    void AdvanceAbility() {
        const auto windows=AbilityWindows();
        if(generated.empty()) {
            ability_phase_start=ability_frame; ability_output_step=0;
            ability_recovery=ability_stage==int(windows.size())-1;
            Generate(0,desired_heading,{});
            ++stats.ability_model_targets;
            if(ability_recovery) ++stats.ability_recoveries;
        }
        // The sole action-pose producer is the decoded model segment. Reference
        // animation frames are read only by AbilityTarget(), never assigned here.
        ++ability_output_step;
        const int duration=ability==0 ? std::array<int,4>{6,4,6,8}[ability_stage] : int(generated.size()-1);
        const float sample=std::min(float(ability_output_step)/duration,1.0f)*float(generated.size()-1);
        segment=std::min(std::size_t(sample),generated.size()-1);
        current=generated[segment];
        const auto& next=generated[std::min(segment+1,generated.size()-1)];
        const float fraction=sample-float(segment);
        for(int j=0;j<77;++j) current.pose[j]=asset::BlendLocalPose(current.pose[j],next.pose[j],fraction);
        for(int j=0;j<4;++j) current.contacts[j]=current.contacts[j]*(1-fraction)+next.contacts[j]*fraction;
        ++stats.ability_model_frames;
        const auto& clip=action_clips[AbilityClipIndex()];
        ability_frame=ability_phase_start+int(float(windows[ability_stage]-ability_phase_start)*sample/float(generated.size()-1));
        if(!ability_hit && clip.active_start>=0 && ability_frame>=clip.active_start) {++stats.strike_events;ability_hit=true;}
        if(segment>=generated.size()-1) {
            generated.clear();
            if(ability==0 && ability_stage==2 && combo_queued && combo_index<2) {
                ++combo_index; combo_queued=false; ability_stage=0; ability_frame=0;
                ability_recovery=false; ability_hit=false;
                return;
            }
            if(++ability_stage>=int(windows.size())) {
                ++stats.ability_completed[ability]; ++stats.completed_actions;
                ability=-1; ability_recovery=false;last_mode=-1;last_request=100;
                StartLoop(selected_preset==2 ? 1 : 0);
            }
        }
    }
    bool preserve_target_group = true;
    auto ToWorld(const SomaPose& pose) const -> SomaPose {
        SomaPose result;
        for (int i = 0; i < 77; ++i) result[i] = source[i].parent < 0 ? pose[i] : Compose(result[source[i].parent], pose[i]);
        return result;
    }
    auto ToLocal(const SomaPose& pose) const -> SomaPose {
        SomaPose result;
        for (int i = 0; i < 77; ++i) result[i] = source[i].parent < 0 ? pose[i] : Relative(pose[source[i].parent], pose[i]);
        return result;
    }
    auto Heading(const SomaPose& pose) const -> float {
        Vec across        = pose[model_to_source[26]].position - pose[model_to_source[22]].position;
        across.y          = 0;
        const Vec forward = math::cross(Vec{0, 1, 0}, across);
        return std::atan2(forward.x, forward.z);
    }
    auto ObjectOffset(int preset) const -> Vec { return {float((preset-4)*4-6)/scale,0,0}; }
    void StartLoop(int index) {
        looping=true; loop_index=index; loop_frame=0;
        // The loop introduces a new support pose. Contact probabilities can stay
        // high across this boundary, so their rising edge cannot renew anchors.
        idle_support_pending=true;
        idle_support_time=0;
        const auto pose=ToWorld(current.pose);
        const auto& first=idle_loops[index][0];
        loop_placement.rotation=Yaw(Heading(pose)-Heading(first));
        const Vec root=Rotate(loop_placement.rotation,first[0].position);
        loop_placement.position={pose[0].position.x-root.x,0,pose[0].position.z-root.z};
        last_mode=0;
    }
    void SampleLoop() {
        const auto& loop=idle_loops[loop_index];
        current.pose=ToLocal(loop[loop_frame%loop.size()]);
        current.pose[0]=Compose(loop_placement,current.pose[0]);
        current.contacts={1,1,1,1};
        ++loop_frame;
    }
    auto StyleTarget(int mode, Vec direction) const -> PresetTarget {
        const int group=selected_preset<4 ? selected_preset : 0;
        const auto& preset=presets[group];
        if(!mode) {
            PresetTarget t; t.name="Idle transition"; t.count=4; t.early=0; t.minimum_tokens=6; t.maximum_tokens=8;
            const auto& loop=idle_loops[group==2 ? 1 : 0];
            for(int f=0;f<4;++f) t.poses[f]=loop[f];
            return t;
        }
        if(group==0) return preset.targets[(mode==2 ? 3 : variant+1)];
        if(group==3) return preset.targets[variant+1];
        // Cardinal blend in character-facing coordinates. Blend local joint
        // transforms, then run FK; mixing world joint positions changes bones.
        const Vec local_direction=Rotate(Yaw(-desired_heading),direction);
        const int forward=local_direction.z>=0 ? 1 : 2, side=local_direction.x>=0 ? 4 : 3;
        const float amount=std::abs(local_direction.x)/(std::abs(local_direction.x)+std::abs(local_direction.z)+1e-8f);
        PresetTarget t=preset.targets[forward];
        for(int f=0;f<4;++f) {
            auto a=ToLocal(t.poses[f]); const auto b=ToLocal(preset.targets[side].poses[f]);
            for(int j=0;j<77;++j) a[j]=asset::BlendLocalPose(a[j],b[j],amount);
            t.poses[f]=ToWorld(a);
        }
        return t;
    }
    // Match feet/knees in a heading-independent frame, including their velocity
    // to distinguish the two passages through similar leg configurations.
    auto GaitFeatures(const SomaPose& pose) const -> std::array<Vec,4> {
        std::array<Vec,4> features;
        const auto turn=Yaw(-Heading(pose));
        const std::array<int,4> joints{23,24,27,28};
        for(int i=0;i<4;++i) features[i]=Rotate(turn,pose[model_to_source[joints[i]]].position-pose[0].position);
        return features;
    }
    auto ForwardBlend() const -> std::pair<int,float> {
        const int low=forward_speed<forward_cycles[1].speed ? 0 : 1;
        return {low,std::clamp((forward_speed-forward_cycles[low].speed)/(forward_cycles[low+1].speed-forward_cycles[low].speed),0.0f,1.0f)};
    }
    void TrackGaitPhase(int index, const SomaPose& now, const SomaPose& previous_pose) {
        const auto& cycle=forward_cycles[index];
        const auto wrap=[&](float value) { return value-std::floor(value/cycle.period)*cycle.period; };
        const bool acquire=phase_cycle<0 || (phase_cycle!=index && (phase_cycle>=3 || index>=3));
        float mapped=phase_position;
        if(!acquire && phase_cycle!=index) {
            const auto& old=forward_cycles[phase_cycle];
            mapped=(phase_position-old.origin)/old.period*cycle.period+cycle.origin;
        }
        const float predicted=acquire ? 0 : mapped+float(phase_clock-previous_phase_clock)*cycle.period;
        const auto observed=GaitFeatures(now), previous_features=GaitFeatures(previous_pose);
        float best_score=std::numeric_limits<float>::max(), best_phase=0;
        for(int f=0;f<cycle.period;++f) {
            const float error=wrap(float(f)-predicted+cycle.period*.5f)-cycle.period*.5f;
            if(!acquire && std::abs(error)>cycle.period*.25f) continue;
            const auto a=GaitFeatures(cycle.poses[f]), b=GaitFeatures(cycle.poses[f+1]);
            float score=0;
            for(int j=0;j<4;++j) {
                const auto p=a[j]-observed[j];
                const auto v=(b[j]-a[j]-(observed[j]-previous_features[j]))*30;
                score+=math::dot(p,p)+.0025f*math::dot(v,v);
            }
            if(!acquire) score+=.0002f*error*error;
            if(score<best_score) { best_score=score; best_phase=acquire ? float(f) : predicted+error; }
        }
        diagnostic_phase_adjustment=acquire ? 0 : std::clamp(best_phase-predicted,-.25f,.25f);
        phase_position=wrap(acquire ? best_phase : predicted+diagnostic_phase_adjustment);
        phase_cycle=index; phase_step=stats.steps; previous_phase_clock=phase_clock;
    }
    auto GaitTarget(int index, PresetTarget target_pose) const -> PresetTarget {
        const auto& cycle=forward_cycles[index];
        // The position spring and phase use the same one-second lookahead.
        const auto& tracked=forward_cycles[phase_cycle];
        const float mapped=(phase_position-tracked.origin)/tracked.period*cycle.period+cycle.origin;
        float start=std::fmod(mapped+30.0f*cycle.period/effective_period,float(cycle.period));
        if(start<0) start+=cycle.period;
        for(int f=0;f<4;++f) {
            const float sample=start+float(f)*cycle.period/effective_period;
            const int base=int(sample); const float blend=sample-base;
            auto a=ToLocal(cycle.poses[base]); const auto b=ToLocal(cycle.poses[base+1]);
            for(int j=0;j<77;++j) a[j]=asset::BlendLocalPose(a[j],b[j],blend);
            target_pose.poses[f]=ToWorld(a);
        }
        return target_pose;
    }
    void Generate(int mode, float heading, Vec direction) {
        std::array<SomaPose, 8> guides;
        for (int f = 0; f < 4; ++f) guides[f] = ToWorld(history[f]);
        PresetTarget target_pose=ability>=0 ? AbilityTarget() : action ? presets[action_preset].targets[presets[action_preset].sequences[sequence_index].targets[action_step]] : StyleTarget(mode,direction);
        const bool use_phase=mode && !action && (selected_preset==0 || selected_preset==3) && preserve_target_group && (gait_probe.empty() || gait_probe.contains("phasefix"));
        diagnostic_phase_cycle=-1; diagnostic_phase_adjustment=0;
        if(use_phase) {
            const auto [low,blend]=ForwardBlend();
            const int index=selected_preset==3 ? 3+variant : blend<.5f ? low : low+1;
            TrackGaitPhase(index,guides[3],guides[2]);
            diagnostic_speed_blend=selected_preset==0 ? blend : 0;
            if(selected_preset==3) target_pose=GaitTarget(index,std::move(target_pose));
            else {
                auto a=GaitTarget(low,target_pose), b=GaitTarget(low+1,target_pose);
                // Align both banks at the first target, then blend local poses.
                // Phase origins mark the same left-foot event across the banks.
                const Joint align_a{.position={a.poses[0][0].position.x,0,a.poses[0][0].position.z},.rotation=Yaw(Heading(a.poses[0]))};
                const Joint align_b{.position={b.poses[0][0].position.x,0,b.poses[0][0].position.z},.rotation=Yaw(Heading(b.poses[0]))};
                for(int f=0;f<4;++f) {
                    for(auto& j:a.poses[f]) j=Relative(align_a,j);
                    for(auto& j:b.poses[f]) j=Relative(align_b,j);
                    auto pa=ToLocal(a.poses[f]); const auto pb=ToLocal(b.poses[f]);
                    for(int j=0;j<77;++j) pa[j]=asset::BlendLocalPose(pa[j],pb[j],blend);
                    target_pose.poses[f]=ToWorld(pa);
                }
            }
            diagnostic_phase_cycle=index;
        } else phase_cycle=-1;
        if(mode && !gait_cycle.empty()) for(int f=0;f<4;++f) target_pose.poses[f]=gait_cycle[stats.steps%40+f];
        if(mode && gait_probe.contains("two")) target_pose.count=2;
        if(mode && gait_probe.contains("max16")) target_pose.maximum_tokens=16;
        active_early=action ? target_pose.early : 0;
        active_pins=action ? target_pose.pins : 0;
        if(action) ++stats.action_targets;
        diagnostic_mask={1,1,1,1,0,0,0,0}; diagnostic_velocity_mask={1,1,1,0,0,0,0,0};
        for(int f=0;f<target_pose.count;++f) {diagnostic_mask[f+4]=1; if(f+1<target_pose.count) diagnostic_velocity_mask[f+4]=1;}
        diagnostic_allowed={};
        for(int n=target_pose.minimum_tokens;n<=target_pose.maximum_tokens;++n) diagnostic_allowed[n-6]=1;
        // Paper section 6.1, equation (6): a one-second, critically damped
        // spring from the current root state toward commanded position/heading.
        // Gamma is a controller tuning value, not a published trained parameter.
        constexpr float gamma = 4.0f;
        const float decay = std::exp(-gamma);
        const std::array<float,6> style_speeds{1.4f,1.25f,.75f,.75f,.8f,.9f};
        const float speed = mode==0 ? 0 : selected_preset==3 ? style_speeds[variant] : selected_preset==2 ? (mode==2 ? 2.5f : 1.5f) : selected_preset==1 ? (mode==2 ? 3.0f : 2.3f) : (mode==2 ? 4.5f : std::array<float,3>{2.3f,3.0f,4.5f}[variant]);
        const Vec command_velocity = direction * (use_phase ? selected_preset==0 ? forward_speed : forward_cycles[phase_cycle].speed : gait_probe.contains("speed") && mode ? 1.152f : speed);
        diagnostic_command_velocity=command_velocity;
        Vec velocity = (guides[3][0].position - guides[2][0].position) * 30;
        velocity.y = 0;
        const Vec naive = guides[3][0].position + command_velocity;
        const Vec displacement = guides[3][0].position - naive;
        const Vec destination = naive + (displacement + velocity + displacement * gamma) * decay;
        const auto angle_delta = [](float a) { return std::atan2(std::sin(a), std::cos(a)); };
        const float current_heading = Heading(guides[3]);
        const float angular_velocity = angle_delta(current_heading - Heading(guides[2])) * 30;
        const float heading_error = -angle_delta(heading - current_heading);
        const float target_heading = current_heading - heading_error +
                                     (heading_error + angular_velocity + gamma * heading_error) * decay;
        const float       delta       = target_heading - Heading((selected_preset==1 || selected_preset==2) && mode ? presets[selected_preset].targets[0].poses[0] : target_pose.poses[0]);
        const Quat        turn        = Yaw(delta);
        for (int f = 0; f < 4; ++f) {
            guides[f + 4] = target_pose.poses[std::min(f,target_pose.count-1)];
            if(ability>=0) continue;
            if(action) {
                for(auto& joint:guides[f+4]) joint=Compose(action_placement,joint);
                continue;
            }
            // Navigation places the whole group; phase-aware forward motion
            // uses the bank's mean speed and preserves its four-frame dynamics.
            const Vec original_root=guides[f+4][0].position;
            for (auto& joint : guides[f + 4]) {
                if (preserve_target_group && mode) {
                    // One rigid transform for the whole sampled group. Keep all
                    // inter-frame displacements, including vertical root motion.
                    const Vec anchor = target_pose.poses[0][0].position;
                    joint.position = Rotate(turn, joint.position - anchor) + Vec{destination.x, anchor.y, destination.z};
                } else {
                    joint.position = Rotate(turn, joint.position - original_root) + Vec{destination.x, original_root.y, destination.z} + command_velocity*(float(f)/30);
                }
                joint.rotation = turn * joint.rotation;
            }
        }
        if (mode) stats.minimum_locomotion_condition_speed = std::min(stats.minimum_locomotion_condition_speed, command_velocity.norm());
        diagnostic_guides = guides;
        Joint                   alignment{.position = Vec{guides[3][0].position.x, 0, guides[3][0].position.z},
                                          .rotation = Yaw(Heading(guides[3]) - std::numbers::pi_v<float> / 2)};
        std::array<float, 40>   roots{};
        std::array<float, 32>   velocities{};
        std::array<float, 2136> poses{};
        auto                    aligned = guides;
        for (auto& p : aligned)
            for (auto& j : p) j = Relative(alignment, j);
        for (int f = 0; f < 8; ++f) {
            const int                  next = (f == 3 || f == 7) ? f : f + 1;
            const Vec                  p = aligned[f][0].position, n = aligned[next][0].position;
            const float                h = Heading(aligned[f]), nh = Heading(aligned[next]);
            const std::array<float, 5> r{p.x, p.y, p.z, std::cos(h), std::sin(h)};
            std::ranges::copy(r, roots.begin() + f * 5);
            const std::array<float, 4> v{std::atan2(std::sin(nh - h), std::cos(nh - h)) * 30, (n.x - p.x) * 30, (n.z - p.z) * 30, p.y};
            std::ranges::copy(v, velocities.begin() + f * 4);
            for (int j = 0; j < 30; ++j) {
                const auto& joint = aligned[f][model_to_source[j]];
                if (j) {
                    const Vec p0 = joint.position - Vec{p.x, 0, p.z};
                    for (int a = 0; a < 3; ++a) poses[f * 267 + (j - 1) * 3 + a] = p0[a];
                }
                const Vec x = Rotate(joint.rotation, Vec{1, 0, 0}), y = Rotate(joint.rotation, Vec{0, 1, 0});
                for (int a = 0; a < 3; ++a) {
                    poses[f * 267 + 87 + j * 6 + a] = x[a];
                    poses[f * 267 + 90 + j * 6 + a] = y[a];
                }
            }
        }
        std::array<float, 23424> output{};
        diagnostic_roots = roots; diagnostic_velocities = velocities; diagnostic_poses = poses;
        int                      tokens = 0;
        const auto               begin  = std::chrono::steady_clock::now();
        inference.Run(roots, velocities, poses, output, tokens, diagnostic_mask, diagnostic_velocity_mask, diagnostic_allowed);
        stats.predicted_tokens = tokens;
        stats.minimum_tokens = std::min(stats.minimum_tokens, tokens);
        stats.maximum_tokens = std::max(stats.maximum_tokens, tokens);
        stats.inference_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        ++stats.inferences;
        generated.resize(tokens * 4 - 6);
        auto write_probe = [&](std::size_t f, std::string_view stage, const SomaPose& pose) {
            if(!gait_plans.is_open()) return;
            for(int side=0;side<2;++side) {
                const auto r=pose[0].position, p=pose[model_to_source[side==0 ? 24 : 28]].position;
                gait_plans << stats.inferences << ',' << f << ',' << stage << ',' << side << ',' << r.x << ',' << r.y << ',' << r.z << ',' << p.x << ',' << p.y << ',' << p.z << ',' << generated[f].contacts[side*2] << '\n';
            }
        };
        diagnostic_raw_roots.resize(generated.size());
        for (std::size_t f = 0; f < generated.size(); ++f) {
            const float* row = output.data() + (f + 3) * 366;
            SomaPose     world_pose;
            for (int j = 0; j < 77; ++j) {
                const auto& s = source[j];
                if (s.model < 0)
                    world_pose[j] = s.parent < 0 ? s.rest : Compose(world_pose[s.parent], s.rest);
                else {
                    world_pose[j].rotation = alignment.rotation * Rotation6D(row + 92 + s.model * 6);
                    world_pose[j].position = s.parent < 0 ? alignment.position + Rotate(alignment.rotation, Vec{row[0], row[1], row[2]}) : world_pose[s.parent].position + Rotate(world_pose[s.parent].rotation, s.rest.position);
                }
            }
            generated[f].pose = ToLocal(world_pose);
            diagnostic_raw_roots[f] = world_pose[0].position;
            for (int c = 0; c < 4; ++c) generated[f].contacts[c] = std::clamp(row[362 + c], 0.0f, 1.0f);
            write_probe(f,"raw_fk",world_pose);
            auto predicted=world_pose;
            for(int j=1;j<30;++j) predicted[model_to_source[j]].position=alignment.position+Rotate(alignment.rotation,Vec{row[5+(j-1)*3]+row[0],row[6+(j-1)*3],row[7+(j-1)*3]+row[2]});
            write_probe(f,"raw_positions",predicted);
        }
        // Match both endpoints, as in the upstream SOMA runtime. Positions remain
        // in world space for the root; child offsets are the reference skeleton.
        const SomaPose end   = ToLocal(guides[4]);
        const SomaPose first = generated.front().pose, last = generated.back().pose;
        for (std::size_t f = 0; f < generated.size(); ++f) {
            const float t = float(f) / float(generated.size() - 1);
            for (int j = 0; j < 77; ++j) {
                auto& p    = generated[f].pose[j];
                if(!gait_probe.contains("raw")) {
                    p.rotation = p.rotation * Mix(Quat::identity(), Inverse(first[j].rotation) * history[3][j].rotation, 1 - t) *
                                 Mix(Quat::identity(), Inverse(last[j].rotation) * end[j].rotation, gait_probe.contains("startonly") ? 0 : t);
                    if (j == 0) p.position = p.position + (history[3][j].position - first[j].position) * (1 - t) + (end[j].position - last[j].position) * (gait_probe.contains("startonly") ? 0 : t);
                }
            }
            write_probe(f,"corrected",ToWorld(generated[f].pose));
        }
        segment = 0;
    }
    void Bind(asset::Scene& scene) {
        auto& entities = scene.GetWorld().GetEntityManager();
        // Release animation ownership entirely: pausing a player would still
        // overwrite procedural transforms with its baked pose each tick.
        std::pmr::vector<ecs::Entity> animated;
        for (auto e : entities.GetEntities<asset::AnimationTarget>()) animated.push_back(e);
        for (auto e : animated) e.Remove<asset::AnimationTarget>();
        for (auto& t : target) {
            t.entity = {};
            for (auto e : entities.GetEntities<asset::MetaInfo, asset::Transform>())
                if (e.Get<asset::MetaInfo>().name == t.name) {
                    bool skeleton = false;
                    for (auto ancestor = e; ancestor.Valid() && ancestor.Has<asset::RelationShip>(); ancestor = ancestor.Get<asset::RelationShip>().parent)
                        if (ancestor.Get<asset::MetaInfo>().name == "__joints") {
                            skeleton = true;
                            break;
                        }
                    if (!skeleton) continue;
                    if (t.entity.Valid()) throw std::runtime_error("Ambiguous target bone name: " + std::string(t.name));
                    t.entity = e;
                }
            if (!t.entity.Valid()) throw std::runtime_error("Runtime scene does not match calibration: " + std::string(t.name));
            const auto& transform = t.entity.Get<asset::Transform>();
            if ((math::get_translation(transform.world_matrix) - t.bind.position).norm() > .001f)
                throw std::runtime_error("Runtime rest pose does not match calibration: " + std::string(t.name));
        }
        bound = &scene;
    }
    void Forward() {
        for (std::size_t i = 0; i < target.size(); ++i) world[i] = target[i].parent < 0 ? local[i] : Compose(world[target[i].parent], local[i]);
    }
    void SetRotation(int i, Quat q) {
        local[i].rotation = target[i].parent < 0 ? q : Inverse(world[target[i].parent].rotation) * q;
        Forward();
    }
    void Retarget(const Frame& frame, float dt) {
        const auto soma = ToWorld(frame.pose);
        diagnostic_display = soma;
        for (std::size_t i = 0; i < target.size(); ++i) {
            const auto& t = target[i];
            local[i]      = t.rest;
            if (t.source >= 0) {
                const Quat rotation = soma[t.source].rotation * t.correction;
                local[i].rotation   = t.parent < 0 ? rotation : Inverse(world[t.parent].rotation) * rotation;
                if (t.source == 0) {
                    const Vec root     = t.bind.position + Vec{soma[0].position.x, soma[0].position.y - source[0].rest.position.y, soma[0].position.z} * scale;
                    stats.display_root = root;
                    local[i].position  = t.parent < 0 ? root : Rotate(Inverse(world[t.parent].rotation), root - world[t.parent].position);
                }
            }
            world[i] = t.parent < 0 ? local[i] : Compose(world[t.parent], local[i]);
        }
        if(!AbilityAirborne()) {
        std::array<Joint, 2> shoe_goals;
        if(!looping) {idle_support_pending=false; idle_support_time=0;}
        if(idle_support_pending && loop_frame>=2) {
            for(int side=0;side<2;++side) {
                const auto& f=feet[side];
                const auto ankle=world[f.ankle];
                const std::array points{f.heel,f.toe};
                for(int c=0;c<2;++c) {
                    idle_support_to[side][c]=ankle.position+Rotate(ankle.rotation,points[c]);
                    idle_support_to[side][c].y=0;
                    idle_support_from[side][c]=f.planted[c] ? f.anchors[c] : idle_support_to[side][c];
                }
            }
            idle_support_pending=false;
            idle_support_time=.35f;
        }
        if(idle_support_time>0) {
            idle_support_time=std::max(0.0f,idle_support_time-dt);
            const float u=1-idle_support_time/.35f;
            const float blend=u*u*(3-2*u);
            for(int side=0;side<2;++side) for(int c=0;c<2;++c)
                feet[side].anchors[c]=idle_support_from[side][c]*(1-blend)+idle_support_to[side][c]*blend;
        }
        float                pelvis_drop = 0;
        float                vertical_overreach = 0;
        for (int side = 0; side < 2; ++side) {
            auto&                    f     = feet[side];
            const auto               ankle = world[f.ankle];
            const std::array<Vec, 2> points{f.heel, f.toe};
            for (int contact = 0; contact < 2; ++contact) {
                const bool planted = frame.contacts[side * 2 + contact] > (f.planted[contact] ? .35f : .65f);
                if (planted && !f.planted[contact]) {
                    f.anchors[contact]   = ankle.position + Rotate(ankle.rotation, points[contact]);
                    f.anchors[contact].y = 0;
                }
                f.planted[contact] = planted;
                f.weights[contact] += std::clamp((planted ? 1.0f : 0.0f) - f.weights[contact], -dt * 12, dt * 12);
            }
            Quat        shoe_rotation = ankle.rotation;
            const float both          = std::min(f.weights[0], f.weights[1]);
            if (both > 0) {
                const Vec span    = Rotate(shoe_rotation, f.toe - f.heel);
                Vec       support = f.anchors[1] - f.anchors[0];
                support.y         = 0;
                if (support.norm() > .001f) shoe_rotation = Mix(shoe_rotation, Arc(span, support) * shoe_rotation, both);
            }
            Vec         desired = ankle.position;
            const float sum = f.weights[0] + f.weights[1], weight = std::max(f.weights[0], f.weights[1]);
            if (sum > 0) {
                const Vec locked = ((f.anchors[0] - Rotate(shoe_rotation, f.heel)) * f.weights[0] + (f.anchors[1] - Rotate(shoe_rotation, f.toe)) * f.weights[1]) / sum;
                desired          = desired * (1 - weight) + locked * weight;
            }
            desired.y += std::max(0.0f, -std::min((desired + Rotate(shoe_rotation, f.heel)).y, (desired + Rotate(shoe_rotation, f.toe)).y));
            const float length   = (world[f.knee].position - world[f.hip].position).norm() + (ankle.position - world[f.knee].position).norm() - .001f;
            // Contact predictions can remain active after the body has moved
            // beyond a planted foot. Do not satisfy that stale anchor by
            // crouching: relax horizontal support to the reachable boundary.
            // Three centimetres covers this character's shoe-height correction.
            constexpr float maximum_shoe_drop = .03f;
            const Vec hip = world[f.hip].position;
            // A generated root can also be ABOVE a straight leg's reachable
            // height. Removing that excess is not bending the knees. Keep the
            // 3 cm crouch budget separate from this vertical reach correction.
            const float excess = weight > .999f ? std::max(0.0f, hip.y - desired.y - length) : 0.0f;
            vertical_overreach = std::max(vertical_overreach, excess);
            const float height = std::max(0.0f, hip.y - maximum_shoe_drop - excess - desired.y);
            const float radius = std::sqrt(std::max(0.0f, length * length - height * height));
            const Vec horizontal{desired.x - hip.x, 0, desired.z - hip.z};
            if (horizontal.norm() > radius && horizontal.norm() > 1e-6f) {
                const Vec correction = horizontal * (radius / horizontal.norm() - 1);
                desired += correction;
                // Move both landmarks together so their shoe orientation stays
                // consistent and the next frame does not restore a stale goal.
                for (auto& anchor : f.anchors) anchor += correction;
            }
            shoe_goals[side] = {.position = desired, .rotation = shoe_rotation};
            const Vec reach = hip - desired;
            const float vertical = std::sqrt(std::max(0.0f, length * length - reach.x * reach.x - reach.z * reach.z));
            pelvis_drop          = std::max(pelvis_drop, reach.y - vertical);
        }
        // Different shoe geometry changes reachable ankle height. Lower the
        // pelvis just enough to keep support goals within the target leg length.
        stats.pelvis_drop = std::min(std::max(pelvis_drop, 0.0f), vertical_overreach + .03f);
        stats.maximum_pelvis_compression = std::max(stats.maximum_pelvis_compression, std::max(0.0f, stats.pelvis_drop - vertical_overreach));
        if (pelvis_drop > 0) {
            pelvis_drop = stats.pelvis_drop;
            stats.maximum_pelvis_drop = std::max(stats.maximum_pelvis_drop, pelvis_drop);
            for (std::size_t i = 0; i < target.size(); ++i)
                if (target[i].source == 0) {
                    const Vec delta{0, -pelvis_drop, 0};
                    local[i].position += target[i].parent < 0 ? delta : Rotate(Inverse(world[target[i].parent].rotation), delta);
                    stats.display_root += delta;
                    break;
                }
            Forward();
        }
        for (int side = 0; side < 2; ++side) {
            const auto& f             = feet[side];
            const auto  ankle         = world[f.ankle];
            const auto  desired       = shoe_goals[side].position;
            const auto  shoe_rotation = shoe_goals[side].rotation;
            const float both          = std::min(f.weights[0], f.weights[1]);
            const Vec   hip = world[f.hip].position, knee = world[f.knee].position;
            const float a = (knee - hip).norm(), b = (ankle.position - knee).norm();
            Vec         direction = desired - hip;
            const float distance  = std::clamp(direction.norm(), std::abs(a - b) + 1e-4f, a + b - 1e-4f);
            if (direction.norm() < 1e-6f) direction = {0, -1, 0};
            direction = math::normalize(direction);
            // Preserve the incoming bend plane, not the knee's offset from the
            // NEW ankle target: shifting that target can reverse the offset.
            // Near extension the plane is ill-conditioned; use body-relative
            // forward and keep the pole in its forward 120-degree cone.
            const Vec forward = Rotate(Yaw(Heading(soma)), Vec{0, 0, 1});
            Vec anatomical = forward - direction * math::dot(forward, direction);
            if (anatomical.norm() < .001f)
                anatomical = math::cross(Rotate(Yaw(Heading(soma)), Vec{1, 0, 0}), direction);
            anatomical = math::normalize(anatomical);
            const Vec original_axis = math::normalize(ankle.position - hip);
            Vec pole = knee - hip - original_axis * math::dot(knee - hip, original_axis);
            const float confidence = std::clamp(pole.norm() / .03f, 0.0f, 1.0f);
            pole -= direction * math::dot(pole, direction);
            if (pole.norm() > 1e-6f) pole = math::normalize(pole);
            if (math::dot(pole, anatomical) < .5f) {
                Vec lateral = pole - anatomical * math::dot(pole, anatomical);
                pole = lateral.norm() > 1e-6f ? anatomical * .5f + math::normalize(lateral) * .8660254f : anatomical;
            }
            pole = math::normalize(anatomical * (1 - confidence) + pole * confidence);
            const float along       = (a * a + distance * distance - b * b) / (2 * distance);
            const Vec   solved_knee = hip + direction * along + pole * std::sqrt(std::max(0.0f, a * a - along * along));
            SetRotation(f.hip, Arc(knee - hip, solved_knee - hip) * world[f.hip].rotation);
            SetRotation(f.knee, Arc(world[f.ankle].position - world[f.knee].position, desired - world[f.knee].position) * world[f.knee].rotation);
            SetRotation(f.ankle, shoe_rotation);
            const Vec leg_axis = math::normalize(world[f.ankle].position - world[f.hip].position);
            const Vec bend = world[f.knee].position - world[f.hip].position;
            const Vec forward_plane = math::normalize(forward - leg_axis * math::dot(forward, leg_axis));
            stats.minimum_knee_forward = std::min(stats.minimum_knee_forward, math::dot(bend, forward_plane));
            ++stats.knee_samples;
            const float heel_height     = (world[f.ankle].position + Rotate(shoe_rotation, f.heel)).y;
            const float forefoot_height = (world[f.ankle].position + Rotate(shoe_rotation, f.toe)).y;
            stats.minimum_sole          = std::min(stats.minimum_sole, std::min(heel_height, forefoot_height));
            if (both > .999f) {
                stats.maximum_double_support_gap = std::max(stats.maximum_double_support_gap, std::max(std::abs(heel_height), std::abs(forefoot_height)));
                ++stats.double_support_samples;
            }
        }
        } else {
            stats.pelvis_drop=0;
            ++stats.airborne_render_frames;
            for(auto& f:feet) {f.planted={}; f.weights={};}
        }
        // Authored hand pins are eased in near the target, as in the official
        // target-pose corrector. Solve in the character's calibrated proportions.
        if(action && active_pins && generated.size()>1) {
            const float progress=float(segment)/float(generated.size()-1-std::min<std::size_t>(active_early,generated.size()-2));
            const float weight=std::clamp((progress-.5f)*2,0.0f,1.0f);
            Vec offset{};
            for(const auto& t:target) if(t.source==0) {offset=t.bind.position-Vec{0,source[0].rest.position.y*scale,0}; break;}
            for(int side=0;side<2;++side) if(active_pins&(1<<side)) {
                const int first=side==0 ? 11 : 17;
                std::array<int,3> chain{-1,-1,-1};
                for(int j=0;j<3;++j) for(int t=0;t<int(target.size());++t) if(target[t].source==model_to_source[first+j]) {chain[j]=t;break;}
                if(std::ranges::find(chain,-1)!=chain.end()) throw std::runtime_error("Missing authored hand pin chain");
                const Vec hip=world[chain[0]].position, mid=world[chain[1]].position, tip=world[chain[2]].position;
                const Vec desired=tip*(1-weight)+(diagnostic_guides[4][model_to_source[first+2]].position*scale+offset)*weight;
                const float a=(mid-hip).norm(), b=(tip-mid).norm();
                Vec axis=desired-hip;
                if(axis.norm()<1e-6f) continue;
                const float distance=std::clamp(axis.norm(),std::abs(a-b)+1e-4f,a+b-1e-4f);
                axis=math::normalize(axis);
                Vec pole=mid-hip-axis*math::dot(mid-hip,axis);
                if(pole.norm()<1e-5f) pole=math::cross(axis,Vec{0,1,0});
                if(pole.norm()<1e-5f) pole=math::cross(axis,Vec{1,0,0});
                pole=math::normalize(pole);
                const float along=(a*a+distance*distance-b*b)/(2*distance);
                const Vec knee=hip+axis*along+pole*std::sqrt(std::max(0.0f,a*a-along*along));
                SetRotation(chain[0],Arc(mid-hip,knee-hip)*world[chain[0]].rotation);
                SetRotation(chain[1],Arc(world[chain[2]].position-world[chain[1]].position,desired-world[chain[1]].position)*world[chain[1]].rotation);
                if(weight>0) ++stats.pinned_frames;
            }
        }
        // Explicit bind-space grants for deformation/helper bones, then their
        // ordinary descendants inherit the updated pose in topological order.
        for (std::size_t i = 0; i < target.size(); ++i) {
            const auto& t = target[i];
            if (t.driver >= 0) {
                const Joint desired = Compose(world[t.driver], Relative(target[t.driver].bind, t.bind));
                local[i]            = t.parent < 0 ? desired : Relative(world[t.parent], desired);
            }
            world[i]     = t.parent < 0 ? local[i] : Compose(world[t.parent], local[i]);
            Joint output = local[i];
            if (t.parent < 0) {
                const auto parent = t.entity.Get<asset::RelationShip>().parent;
                if (parent.Valid()) {
                    const auto& transform = parent.Get<asset::Transform>();
                    const Joint frame{.position = math::get_translation(transform.world_matrix), .rotation = math::get_rotation(transform.world_matrix)};
                    output = Relative(frame, world[i]);
                }
            }
            asset::WriteLocalPose(output, target[i].entity.Get<asset::Transform>());
        }
    }
    Inference                                 inference;
    std::array<ActionClip,5> action_clips;
    int combo_index=0, ability_output_step=0;
    bool combo_queued=false;
    int ability=-1, ability_frame=0, ability_stage=0, ability_phase_start=0;
    bool ability_recovery=false, ability_hit=false;
    Joint ability_placement;
    std::string gait_probe;
    std::ofstream gait_plans;
    std::vector<SomaPose> gait_cycle;
    std::array<GaitCycle,9> forward_cycles;
    int phase_cycle=-1, diagnostic_phase_cycle=-1;
    unsigned phase_step=0;
    float phase_position=0, diagnostic_phase_adjustment=0;
    float forward_speed=0, effective_period=30, diagnostic_speed_blend=0;
    double phase_clock=0, previous_phase_clock=0;
    bool speed_transition=false;
    Vec diagnostic_command_velocity{};
    std::array<SomaPose, 8> diagnostic_guides{};
    SomaPose diagnostic_display{};
    std::pmr::vector<Vec> diagnostic_raw_roots;
    std::array<float,40> diagnostic_roots{};
    std::array<float,32> diagnostic_velocities{};
    std::array<float,2136> diagnostic_poses{};
    std::array<SourceJoint, 77>               source;
    std::array<int, 30>                       model_to_source{};
    std::pmr::vector<TargetJoint>             target;
    std::pmr::vector<Joint>                   local, world;
    std::array<Foot, 2>                       feet;
    std::array<std::pmr::vector<SomaPose>, 3> banks;
    std::array<SomaPose, 4>                   history;
    std::pmr::vector<Frame>                   generated;
    Frame                                     previous, current;
    std::size_t                               segment = 0;
    double                                    accumulator = 0;
    float                                     scale = 1, desired_heading = 0;
    int                                       last_mode = -1, last_request = 100;
    Vec                                       last_direction{};
    asset::Scene*                             bound = nullptr;
    Stats                                     stats;
    std::array<Preset,8> presets;
    std::array<std::vector<SomaPose>,3> idle_loops;
    int selected_preset=0, variant=0, sequence_index=0, loop_index=0;
    int action_preset=4, preferred_interaction=4;
    std::array<int,4> interaction_variants{};
    Joint action_placement;
    bool free_jump=false, turn_requested=false;
    std::size_t action_step=0, loop_frame=0;
    bool action=false, seated=false, looping=false;
    Joint loop_placement;
    bool idle_support_pending=false;
    float idle_support_time=0;
    std::array<std::array<Vec,2>,2> idle_support_from{},idle_support_to{};
    int active_early=0, active_pins=0;
    std::array<int,8> diagnostic_mask{}, diagnostic_velocity_mask{};
    std::array<int,11> diagnostic_allowed{};
};
}  // namespace hitagi::examples::motionbricks
