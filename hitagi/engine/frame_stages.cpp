export module engine:frame_stages;
import ecs;

export namespace hitagi {

// Engine frame policy, independent of the generic ECS scheduling mechanism.
enum class FrameStage { First,
                        PreUpdate,
                        Update,
                        PostUpdate,
                        Render,
                        Last };

void ConfigureFrameStages(ecs::Schedule& schedule);

}  // namespace hitagi

namespace hitagi {

void ConfigureFrameStages(ecs::Schedule& schedule) {
    schedule.SetOrder(FrameStage::First, FrameStage::PreUpdate);
    schedule.SetOrder(FrameStage::PreUpdate, FrameStage::Update);
    schedule.SetOrder(FrameStage::Update, FrameStage::PostUpdate);
    schedule.SetOrder(FrameStage::PostUpdate, FrameStage::Render);
    schedule.SetOrder(FrameStage::Render, FrameStage::Last);
}

}  // namespace hitagi
