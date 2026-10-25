export module core:plugin;
import std;
import utils;
import interop.spdlog;

export namespace hitagi::core {

template <typename Plugin, typename Host>
concept PluginFor = requires(const Plugin& plugin, Host& host) {
    { plugin.Build(host) } -> std::same_as<void>;
};

// Only installation metadata is retained. Runtime state belongs to the host.
template <typename Host>
class PluginSet {
public:
    template <typename Plugin>
        requires PluginFor<Plugin, Host>
    void Add(Host& host, const Plugin& plugin) {
        if (m_Failed) throw std::logic_error("Plugin installation previously failed");
        const auto id   = utils::TypeID::Create<Plugin>();
        const auto name = std::string_view(typeid(Plugin).name());
        if (const auto found = m_Installed.find(id); found != m_Installed.end()) {
            if (!found->second) {
                m_Failed = true;
                std::string chain;
                for (const auto& item : m_Stack) chain += std::string(item) + " -> ";
                throw std::logic_error("Cyclic plugin installation: " + chain + std::string(name));
            }
            throw std::logic_error("Duplicate plugin: " + std::string(name));
        }
        m_Installed.emplace(id, false);
        try {
            m_Stack.emplace_back(name);
            spdlog::info("Installing plugin {}", name);
            plugin.Build(host);
            if (m_Failed) throw std::logic_error("Nested plugin installation failed");
            spdlog::info("Installed plugin {}", name);
            m_Installed.at(id) = true;
            m_Stack.pop_back();
        } catch (...) {
            m_Failed = true;
            if (!m_Stack.empty()) m_Stack.pop_back();
            throw;
        }
    }

    template <typename Plugin>
    bool Has() const noexcept {
        const auto found = m_Installed.find(utils::TypeID::Create<Plugin>());
        return found != m_Installed.end() && found->second;
    }
    bool IsValid() const noexcept { return !m_Failed && m_Stack.empty(); }

private:
    std::pmr::unordered_map<utils::TypeID, bool> m_Installed;
    std::pmr::vector<std::pmr::string>           m_Stack;
    bool                                         m_Failed = false;
};

}  // namespace hitagi::core
