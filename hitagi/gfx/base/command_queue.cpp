export module gfx.base:command_queue;
import std;
import utils;
import magic_enum;
import :command_context;
import :sync;

export namespace hitagi::gfx {

class CommandQueue {
public:
    static auto Create(Device& device, CommandType type, std::string_view name = "") -> std::shared_ptr<CommandQueue>;
    CommandQueue(CommandType type, std::string_view name) : m_Type(type), m_Name(name) {}
    virtual ~CommandQueue() = default;

    inline auto  GetType() const noexcept { return m_Type; }
    inline auto& GetName() const noexcept { return m_Name; }

    virtual void Submit(
        std::span<const std::reference_wrapper<const CommandContext>> contexts,
        std::span<const FenceWaitInfo>                                wait_fences   = {},
        std::span<const FenceSignalInfo>                              signal_fences = {}) = 0;

    virtual void WaitIdle() = 0;
    virtual void NewFrame() {}

protected:
    const CommandType      m_Type;
    const std::pmr::string m_Name;
};

class CommandQueues {
public:
    explicit CommandQueues(Device& device);
    ~CommandQueues();
    CommandQueues(const CommandQueues&)            = delete;
    CommandQueues& operator=(const CommandQueues&) = delete;

    auto Get(CommandType type) const -> CommandQueue& {
        if (!magic_enum::enum_contains(type)) throw std::invalid_argument("Invalid command type");
        return *m_Queues[type];
    }
    void WaitIdle();
    void NewFrame();

private:
    utils::EnumArray<std::shared_ptr<CommandQueue>, CommandType> m_Queues;
};

}  // namespace hitagi::gfx

namespace hitagi::gfx {
CommandQueues::CommandQueues(Device& device) {
    magic_enum::enum_for_each<CommandType>([&](CommandType type) {
        m_Queues[type] = CommandQueue::Create(device, type, std::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type)));
    });
}
CommandQueues::~CommandQueues() { WaitIdle(); }
void CommandQueues::WaitIdle() {
    for (auto& queue : m_Queues) queue->WaitIdle();
}
void CommandQueues::NewFrame() {
    for (auto& queue : m_Queues) queue->NewFrame();
}
}  // namespace hitagi::gfx
