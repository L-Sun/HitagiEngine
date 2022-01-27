#include "MyGame.hpp"
#include "FileIOManager.hpp"
#include "ThreadManager.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <queue>

using namespace Hitagi;

int MyGame::Initialize() {
    int ret  = 0;
    m_Logger = spdlog::stdout_color_mt("MyGame");
    m_Logger->info("Initialize...");

    if ((ret = m_Editor.Initialize()) != 0) return ret;

    std::filesystem::path model_path = "./Assets/DLModels/mann.onnx";
    m_Logger->info("Load MANN ({})", model_path.string());
    m_MANN = std::make_unique<MANN>(model_path);

    m_Clock.Start();
    return ret;
}

void MyGame::Finalize() {
    m_Logger->info("MyGame Finalize");
    m_Logger = nullptr;
}

void MyGame::Tick() {
    m_Clock.Tick();

    m_Editor.Tick();
    for (auto&& [node, dr_character] : m_DLCharacters) {
        dr_character->Tick();
    }

    DrawBone();
    g_GuiManager->DrawGui([&]() {
        m_Editor.Draw();
        bool open = true;
        if (ImGui::Begin("DL Animation Pannel", &open)) {
            if (ImGui::Button("Dump Animation")) {
                DumpAnimation();
            }
            if (ImGui::Button("Animation to DL")) {
                SwitchDL();
            }
        }
        ImGui::End();
    });
}

void MyGame::DrawBone() {
    auto scene = g_SceneManager->GetScene();
    for (auto&& bone : scene->bone_nodes) {
        if (auto parent = bone->GetParent().lock()) {
            vec3f from = (parent->GetCalculatedTransformation() * vec4f(0, 0, 0, 1)).xyz;
            vec3f to   = (bone->GetCalculatedTransformation() * vec4f(0, 0, 0, 1)).xyz;
            g_DebugManager->DrawLine(from, to, vec4f(0.2f, 0.3f, 0.4f, 1.0f));
        }
    }
}

void MyGame::DumpAnimation() {
    auto position_at_frame = [](std::shared_ptr<Asset::Animation> animation, size_t frame) -> std::vector<vec3f> {
        size_t             num_nodes = animation->Data().size();
        std::vector<vec3f> result;
        result.reserve(num_nodes);

        std::shared_ptr<Asset::SceneNode>                 root     = animation->GetSkeleton();
        std::function<void(decltype(root), const mat4f&)> recusive = [&](decltype(root) node, const mat4f& parent_trans) -> void {
            // End joint dose not have data
            if (animation->Data().at(frame).count(node) == 0) return;

            auto  trs            = animation->Data().at(frame).at(node);
            mat4f transform      = parent_trans * translate(rotate(mat4f(1.0f), trs.rotation), trs.translation);
            auto&& [pos, _r, _s] = decompose(transform);
            result.emplace_back(pos);
            for (auto&& child : node->GetChildren()) {
                recusive(child, transform);
            }
        };

        auto  trs       = animation->Data().at(frame).at(root);
        mat4f transform = translate(rotate(mat4f(1.0f), trs.rotation), trs.translation);
        // root parent_trans
        recusive(root, inverse(transform));
        return result;
    };

    auto velocity_at_frame = [&](std::shared_ptr<Asset::Animation> animation, size_t frame) -> std::vector<vec3f> {
        auto last_position = position_at_frame(animation, frame - 1);
        auto curr_position = position_at_frame(animation, frame);

        std::vector<vec3f> result;
        result.reserve(curr_position.size());
        for (size_t i = 0; i < curr_position.size(); i++) {
            result.emplace_back(curr_position[i] - last_position[i]);
        }
        return result;
    };

    auto rotation_at_frame = [](std::shared_ptr<Asset::Animation> animation, size_t frame) -> std::vector<vec3f> {
        size_t             num_nodes = animation->Data().size();
        std::vector<vec3f> result;
        result.reserve(num_nodes);

        std::shared_ptr<Asset::SceneNode>                 root     = animation->GetSkeleton();
        std::function<void(decltype(root), const mat4f&)> recusive = [&](decltype(root) node, const mat4f& parent_trans) -> void {
            // End joint dose not have data
            if (animation->Data().at(frame).count(node) == 0) return;

            auto  trs            = animation->Data().at(frame).at(node);
            mat4f transform      = parent_trans * translate(rotate(mat4f(1.0f), trs.rotation), trs.translation);
            auto&& [_p, rot, _s] = decompose(transform);
            result.emplace_back(rot);
            for (auto&& child : node->GetChildren()) {
                recusive(child, transform);
            }
        };

        auto  trs       = animation->Data().at(frame).at(root);
        mat4f transform = translate(rotate(mat4f(1.0f), trs.rotation), trs.translation);
        // root parent_trans
        recusive(root, inverse(transform));
        return result;
    };

    auto trajectory_change_at_frame = [](std::shared_ptr<Asset::Animation> animation, size_t frame) -> vec2f {
        std::shared_ptr<Asset::SceneNode> root = animation->GetSkeleton();
        return (animation->Data().at(frame).at(root).translation - animation->Data().at(frame - 1).at(root).translation).xy;
    };

    auto facing_change_at_frame = [](std::shared_ptr<Asset::Animation> animation, size_t frame) -> float {
        std::shared_ptr<Asset::SceneNode> root = animation->GetSkeleton();

        vec4f forward(1.0f, 0.0f, 0.0f, 0.0f);
        vec4f last_forward = rotate(mat4f(1.0f), animation->Data().at(frame - 1).at(root).rotation) * forward;
        vec4f curr_forward = rotate(mat4f(1.0f), animation->Data().at(frame).at(root).rotation) * forward;

        float theta = dot(vec3f(last_forward.xyz), vec3f(curr_forward.xyz));
        float sign  = cross(vec3f(last_forward.xy, 0.0f), vec3f(curr_forward.xy, 0.0f)).z;
        return std::copysign(std::acos(theta), sign);
    };

    auto scene = g_SceneManager->GetScene();
    for (auto&& animation : scene->animations) {
        g_ThreadManager->RunTask([&]() {
            std::ofstream result(fmt::format("E:\\workspace\\character_animation\\train_data\\{}.csv", animation->GetName()));
            if (!result.is_open()) {
                throw std::runtime_error(fmt::format("Can't not create file {}", animation->GetName()));
            }

            for (size_t i = 1; i < animation->Data().size(); i++) {
                auto position = position_at_frame(animation, i);
                auto velocity = velocity_at_frame(animation, i);
                auto rotation = rotation_at_frame(animation, i);

                for (size_t j = 0; j < position.size(); j++) {
                    result << fmt::format("{}, {}, {}", position[j].x, position[j].y, position[j].z)
                           << ", "
                           << fmt::format("{}, {}, {}", velocity[j].x, velocity[j].y, velocity[j].z)
                           << ", "
                           << fmt::format("{}, {}, {}", rotation[j].x, rotation[j].y, rotation[j].z)
                           << ", ";
                }
                auto change = trajectory_change_at_frame(animation, i);
                auto facing = facing_change_at_frame(animation, i);
                result << fmt::format("{}, {}", change.x, change.y)
                       << ", "
                       << facing
                       << "\n";
            }
            result.flush();
            result.close();
        });
    }
}

void MyGame::SwitchDL() {
    auto  scene     = g_SceneManager->GetScene();
    auto& anima_mgr = g_SceneManager->GetAnimationManager();

    for (auto&& anima : scene->animations) {
        anima->Pause();
        m_DLCharacters.emplace(
            anima->GetSkeleton(),
            std::make_shared<DLCharacter>(anima->GetSkeleton(), *m_MANN));
    }
}
