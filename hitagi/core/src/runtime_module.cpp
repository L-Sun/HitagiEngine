#include <hitagi/core/runtime_module.hpp>
#include <hitagi/utils/logger.hpp>

#include <spdlog/spdlog.h>

#include <tracy/Tracy.hpp>

namespace hitagi {
std::unordered_map<std::string, RuntimeModule*> RuntimeModule::sm_AllModules;

RuntimeModule::RuntimeModule(std::string_view name)
    : m_Name(name), m_Logger(utils::try_create_logger(name)) {
    const auto message = fmt::format("Initialize {}", m_Name);
    ZoneScoped;
    ZoneName(message.data(), message.size());
    m_Logger->info("Initialize...");

    std::string _name(m_Name);
    if (sm_AllModules.contains(_name)) {
        const auto error_message = fmt::format("Module {} already exists", _name);
        m_Logger->error(error_message);
        throw std::invalid_argument(error_message);
    }
    sm_AllModules.emplace(_name, this);
}

RuntimeModule::~RuntimeModule() {
    while (!m_SubModules.empty()) {
        ZoneScoped;
        const auto& sub_module = m_SubModules.back();
        const auto  message    = fmt::format("Finalize {}", sub_module->GetName());
        ZoneName(message.data(), message.size());
        m_SubModules.pop_back();
    }
    m_Logger->info("Finalize {}", m_Name);
    sm_AllModules.erase(std::string(m_Name));
}

void RuntimeModule::Tick() {
    for (const auto& sub_module : m_SubModules) {
        ZoneScoped;
        ZoneName(sub_module->GetName().data(), sub_module->GetName().size());
        sub_module->Tick();
    }
}

auto RuntimeModule::GetSubModule(std::string_view name) -> RuntimeModule* {
    auto iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [&](auto& _mod) -> bool { return _mod->GetName() == name; });
    return iter != m_SubModules.end() ? (*iter).get() : nullptr;
}

auto RuntimeModule::GetSubModules() const noexcept -> std::pmr::vector<RuntimeModule*> {
    std::pmr::vector<RuntimeModule*> result;
    std::transform(m_SubModules.begin(), m_SubModules.end(), std::back_inserter(result), [](const auto& submodule) { return submodule.get(); });
    return result;
}

auto RuntimeModule::AddSubModule(std::unique_ptr<RuntimeModule> module, RuntimeModule* after) -> RuntimeModule* {
    if (module == nullptr) return nullptr;

    if (after) {
        const auto iter = std::find_if(m_SubModules.begin(), m_SubModules.end(), [=](auto& _mod) -> bool { return _mod.get() == after; });
        if (iter == m_SubModules.end()) {
            const auto error_message = fmt::format("Module {} does not exist in current module({})", after->GetName(), GetName());
            m_Logger->error(error_message);
            throw std::invalid_argument(error_message);
        }
        return m_SubModules.insert(std::next(iter), std::move(module))->get();
    } else {
        m_SubModules.push_back(std::move(module));
        return m_SubModules.back().get();
    }
}

void RuntimeModule::UnloadSubModule(std::string_view name) {
    std::erase_if(m_SubModules, [&](auto& _mod) -> bool { return _mod->GetName() == name; });
}

auto RuntimeModule::GetModule(std::string_view name) -> RuntimeModule* {
    const std::string _name(name);
    if (auto iter = sm_AllModules.find(_name); iter != sm_AllModules.end()) {
        return iter->second;
    } else {
        return nullptr;
    }
}

}  // namespace hitagi