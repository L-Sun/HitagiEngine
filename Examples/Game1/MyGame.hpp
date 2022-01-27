#pragma once
#include "MANN.hpp"
#include "DLCharacter.hpp"

#include "GameLogic.hpp"
#include "Editor.hpp"

class MyGame : public Hitagi::GameLogic {
public:
    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void DrawBone();
    void DumpAnimation();
    void SwitchDL();

private:
    void UpdateVelocities();

    Hitagi::Core::Clock m_Clock;
    bool                m_ShowEditor = true;
    Hitagi::Editor      m_Editor;

    std::unique_ptr<MANN> m_MANN;

    std::unordered_map<std::shared_ptr<Hitagi::Asset::SceneNode>, std::shared_ptr<DLCharacter>> m_DLCharacters;
};