#include "DLCharacter.hpp"

using namespace Hitagi;
DLCharacter::DLCharacter(std::shared_ptr<Asset::SceneNode> skeleton, MANN& mann) : m_Skeleton(skeleton), m_MANN(mann) {
    m_Clock.Start();
}

void DLCharacter::Tick() {
    if (m_Clock.DeltaTime().count() < 0.0333333333) return;
    auto state = GetCharacterState();
    ApplyNewState(m_MANN.Predict(state));
    m_Clock.Tick();
}

std::vector<float> DLCharacter::GetCharacterState() {
    std::vector<float> state;
    mat4f              global_to_root = inverse(m_Skeleton->GetCalculatedTransformation());

    std::function<void(decltype(m_Skeleton) node)> recusive = [&](decltype(m_Skeleton) node) -> void {
        // End joint dose not have data
        if (node->GetChildren().empty()) return;
        auto&& [pos, rot, _s] = decompose(global_to_root * node->GetCalculatedTransformation());
        vec3f velocity        = (global_to_root * vec4f(node->GetVelocity(), 0.0f)).xyz;

        state.emplace_back(pos.x);
        state.emplace_back(pos.y);
        state.emplace_back(pos.z);
        state.emplace_back(velocity.x);
        state.emplace_back(velocity.y);
        state.emplace_back(velocity.z);
        state.emplace_back(rot.x);
        state.emplace_back(rot.y);
        state.emplace_back(rot.z);
        for (auto&& child : node->GetChildren()) {
            recusive(child);
        }
    };
    recusive(m_Skeleton);
    state.emplace_back(root_changed.x);
    state.emplace_back(root_changed.y);
    state.emplace_back(root_changed.z);

    return state;
}

void DLCharacter::ApplyNewState(const std::vector<float>& data) {
    vec3f root_translation = {data.at(data.size() - 3),
                              data.at(data.size() - 2),
                              0.0f};
    float theta            = data.back();

    root_changed = vec3f{root_translation.x, root_translation.y, theta};
    m_Skeleton->Rotate(vec3f(0.0f, 0.0f, theta));
    m_Skeleton->Translate(root_translation);
    m_Skeleton->SetVelocity(root_translation / m_Clock.DeltaTime().count());
    mat4f root_trans = m_Skeleton->GetCalculatedTransformation();

    auto p = reinterpret_cast<const vec3f*>(data.data());

    std::function<void(decltype(m_Skeleton), const mat4f&)> recusive = [&](decltype(m_Skeleton) node, const mat4f& parent_trans) -> void {
        // End joint dose not have data
        if (node->GetChildren().empty()) return;
        auto position = *p++;
        auto velocity = *p++;
        auto rotation = *p++;

        if (node != m_Skeleton) {
            mat4f trans_in_root_space   = translate(rotate(mat4f(1.0f), rotation), position);
            mat4f trans_in_parent_space = inverse(parent_trans) * root_trans * trans_in_root_space;
            auto [pos, rot, _s]         = decompose(trans_in_parent_space);
            // !We do not use the predict velocity!
            node->SetVelocity(velocity);
            // node->SetVelocity((pos - node->GetPosition()) / m_Clock.DeltaTime().count());
            node->SetTRS(pos, euler_to_quaternion(rot), vec3f(1.0f));
        }

        mat4f trans_in_global = node->GetCalculatedTransformation();
        for (auto&& child : node->GetChildren()) {
            recusive(child, trans_in_global);
        }
    };

    recusive(m_Skeleton, mat4f(1.0f));
}