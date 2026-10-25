export module core:runtime;
import std;
import interop.spdlog;
import :memory_manager;
import :io_service;
import :job_system;

namespace hitagi::core::detail {
class RuntimeLogging {
public:
    explicit RuntimeLogging(std::string_view level) : m_Previous(spdlog::get_level()) {
        if (level != "trace" && level != "debug" && level != "info" && level != "warn" && level != "error" && level != "critical" && level != "off")
            throw std::invalid_argument("Invalid runtime log level");
        spdlog::set_level(spdlog::level::from_str(std::string(level)));
        spdlog::info("Initialize Runtime");
    }
    ~RuntimeLogging() {
        spdlog::info("Finalize Runtime");
        spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) { logger->flush(); });
        spdlog::set_level(m_Previous);
    }

private:
    spdlog::level::level_enum m_Previous;
};
}  // namespace hitagi::core::detail

export namespace hitagi::core {
struct RuntimeConfig {
    std::uint32_t    worker_count = 0;
    std::string_view log_level    = "info";
};

// Construct once outside all Apps and their PMR configuration. Destruction joins
// workers before releasing file snapshots and the default memory resource.
class Runtime {
public:
    explicit Runtime(RuntimeConfig config = {})
        : m_Logging(config.log_level), m_Jobs(config.worker_count) {}
    ~Runtime() { m_Jobs.WaitForAll(); }
    Runtime(const Runtime&)            = delete;
    Runtime& operator=(const Runtime&) = delete;

    auto Memory() noexcept -> MemoryManager& { return m_Memory; }
    auto IO() noexcept -> core::IOService& { return m_IO; }
    auto Jobs() noexcept -> JobSystem& { return m_Jobs; }

private:
    detail::RuntimeLogging m_Logging;
    MemoryManager          m_Memory;
    core::IOService        m_IO;
    JobSystem              m_Jobs;
};
}  // namespace hitagi::core
