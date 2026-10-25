export module gfx:graphics_frame;
import gfx.base;
import ecs;

export namespace hitagi::gfx {
// The host owns the facilities and chooses where this work belongs in its schedule.
// Run on the calling thread and call Device::EndFrame after the entire frame,
// including on exceptions; an ordinary final task could be skipped on failure.
class GraphicsFrameSystem {
public:
    GraphicsFrameSystem(Device& device, CommandQueues& queues, ecs::Group group);
    void ConfigureSchedule(ecs::Schedule& schedule);

private:
    Device&        m_Device;
    CommandQueues& m_Queues;
    ecs::Group     m_Group;
};
}  // namespace hitagi::gfx

namespace hitagi::gfx {
GraphicsFrameSystem::GraphicsFrameSystem(Device& device, CommandQueues& queues, ecs::Group group)
    : m_Device(device), m_Queues(queues), m_Group(group) {}

void GraphicsFrameSystem::ConfigureSchedule(ecs::Schedule& schedule) {
    schedule.Request("Graphics.Device", [this] { m_Device.BeginFrame(); }, m_Group);
    schedule.Request("Graphics.Queues", [this] { m_Queues.NewFrame(); }, m_Group);
    schedule.SetOrder("Graphics.Device", "Graphics.Queues");
}
}  // namespace hitagi::gfx
