#pragma once
#include "SceneManager.hpp"

#include "MANN.hpp"

class DLCharacter {
public:
    DLCharacter(std::shared_ptr<Hitagi::Asset::SceneNode> skeleton, MANN& mann);

    void Tick();

private:
    Hitagi::Core::Clock m_Clock;
    void                ApplyNewState(const std::vector<float>& data);
    std::vector<float>  GetCharacterState();

    MANN&                                     m_MANN;
    std::shared_ptr<Hitagi::Asset::SceneNode> m_Skeleton;

    Hitagi::vec3f root_changed{0.0f, 0.0f, 0.0f};
};