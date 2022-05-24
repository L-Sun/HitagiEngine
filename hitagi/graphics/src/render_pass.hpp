#pragma once
#include <utility>

namespace hitagi::graphics {
class ResourceHelper;

class PassExecutor {
public:
    virtual ~PassExecutor()                            = default;
    virtual void Execute(const ResourceHelper& helper) = 0;
};

template <typename PassData, typename ExecuteFunc>
class RenderPass : public PassExecutor {
public:
    RenderPass(ExecuteFunc&& execute) : m_Data{}, m_Execute(std::move(execute)) {}
    void            Execute(const ResourceHelper& helper) final { m_Execute(helper, m_Data); }
    PassData&       GetData() noexcept { return m_Data; }
    const PassData& GetData() const noexcept { return m_Data; }

private:
    PassData    m_Data;
    ExecuteFunc m_Execute;
};

}  // namespace hitagi::graphics
