#pragma once
#include "Image.hpp"
#include "Buffer.hpp"

#include <crossguid/guid.hpp>
#include <fmt/format.h>

#include <filesystem>

namespace Hitagi::Asset {
class SceneObject {
public:
    inline const xg::Guid&    GetGuid() const noexcept { return m_Guid; }
    inline void               SetName(std::string name) noexcept { m_Name = std::move(name); }
    inline const std::string& GetName() const noexcept { return m_Name; }
    std::string               GetUniqueName(std::string_view sep = "") const noexcept { return fmt::format("{}{}{}", m_Name, sep, m_Guid.str()); }

    friend std::ostream& operator<<(std::ostream& out, const SceneObject& obj);

protected:
    xg::Guid    m_Guid;
    std::string m_Name;

    SceneObject() : m_Guid(xg::newGuid()) {}
    SceneObject(const SceneObject& obj) : m_Guid(xg::newGuid()) {}
    SceneObject& operator=(const SceneObject& rhs) {
        if (this != &rhs) {
            m_Name = rhs.m_Name;
        }
        return *this;
    }

    SceneObject(SceneObject&&) = default;
    SceneObject& operator=(SceneObject&&) = default;
};
}  // namespace Hitagi::Asset
