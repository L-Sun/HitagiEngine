#pragma once
#include <hitagi/engine.hpp>

class Playground : public hitagi::RuntimeModule {
public:
    Playground(hitagi::Engine& engine);
    void Tick() final;

    hitagi::Engine&                            engine;
    hitagi::asset::Scene                       scene;
    std::shared_ptr<hitagi::asset::CameraNode> camera;
    std::shared_ptr<hitagi::asset::MeshNode>   cube;
};
