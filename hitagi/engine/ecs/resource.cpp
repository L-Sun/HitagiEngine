export module ecs:resource;
import std;
import utils;

export namespace hitagi::ecs {

template <typename T>
class Res {
public:
    explicit Res(const T& value) : m_Value(&value) {}
    auto operator->() const noexcept -> const T* { return m_Value; }
    auto operator*() const noexcept -> const T& { return *m_Value; }
    using value_type               = T;
    static constexpr bool writable = false;

private:
    const T* m_Value;
};
template <typename T>
class ResMut {
public:
    explicit ResMut(T& value) : m_Value(&value) {}
    auto operator->() const noexcept -> T* { return m_Value; }
    auto operator*() const noexcept -> T& { return *m_Value; }
    using value_type               = T;
    static constexpr bool writable = true;

private:
    T* m_Value;
};
}  // namespace hitagi::ecs

namespace hitagi::ecs::detail {
template <typename T>
inline constexpr bool is_resource = false;
template <typename T>
inline constexpr bool is_resource<Res<T>> = true;
template <typename T>
inline constexpr bool is_resource<ResMut<T>> = true;

class Resources {
    struct Entry {
        std::shared_ptr<void> owner;
        void*                 value;
    };

public:
    ~Resources() { Clear(); }
    template <typename T, typename... Args>
    auto Emplace(Args&&... args) -> T& {
        CheckMutable();
        const auto id = utils::TypeID::Create<T>();
        if (m_Entries.contains(id)) throw std::logic_error("Duplicate resource: " + std::string(typeid(T).name()));
        auto value  = std::make_shared<T>(std::forward<Args>(args)...);
        T&   result = *value;
        Insert(id, {std::move(value), &result});
        return result;
    }
    template <typename T>
    void Borrow(T& value) {
        CheckMutable();
        const auto id = utils::TypeID::Create<T>();
        if (m_Entries.contains(id)) throw std::logic_error("Duplicate resource: " + std::string(typeid(T).name()));
        Insert(id, {{}, &value});
    }
    template <typename T>
    auto Get() -> T& {
        const auto found = m_Entries.find(utils::TypeID::Create<T>());
        if (found == m_Entries.end()) throw std::logic_error("Missing resource: " + std::string(typeid(T).name()));
        return *static_cast<T*>(found->second.value);
    }
    template <typename T>
    auto Get() const -> const T& { return const_cast<Resources*>(this)->Get<T>(); }
    template <typename T>
    bool Has() const { return m_Entries.contains(utils::TypeID::Create<T>()); }
    void BeginExecution() { m_Executing = true; }
    void EndExecution() noexcept { m_Executing = false; }
    void Clear() {
        while (!m_Order.empty()) {
            m_Entries.erase(m_Order.back());
            m_Order.pop_back();
        }
    }

private:
    void CheckMutable() const {
        if (m_Executing) throw std::logic_error("Cannot install resources during World::Update");
    }
    void Insert(utils::TypeID id, Entry entry) {
        m_Order.push_back(id);
        try {
            m_Entries.emplace(id, std::move(entry));
        } catch (...) {
            m_Order.pop_back();
            throw;
        }
    }
    bool                                          m_Executing = false;
    std::pmr::unordered_map<utils::TypeID, Entry> m_Entries;
    std::pmr::vector<utils::TypeID>               m_Order;
};
}  // namespace hitagi::ecs::detail
