#pragma once
#include "HitagiMath.hpp"
#include "SceneObject.hpp"
#include "Bone.hpp"
#include "Buffer.hpp"
#include "Material.hpp"

namespace Hitagi::Asset {

class VertexArray {
public:
    enum struct DataType {
        Float1,
        Float2,
        Float3,
        Float4,
        Double1,
        Double2,
        Double3,
        Double4,
    };

    VertexArray() = default;
    VertexArray(std::string_view attr,
                DataType         data_type,
                Core::Buffer&&   buffer)
        : m_Attribute(attr),
          m_DataType(data_type),
          m_VertexCount(buffer.GetDataSize() / GetVertexSize()),
          m_Data(std::move(buffer)) {}

    inline const std::string& GetAttributeName() const noexcept { return m_Attribute; }
    inline DataType           GetDataType() const noexcept { return m_DataType; }
    inline size_t             GetDataSize() const noexcept { return m_Data.GetDataSize(); }
    inline const uint8_t*     GetData() const noexcept { return m_Data.GetData(); }
    inline size_t             GetVertexCount() const noexcept { return m_VertexCount; }

    size_t               GetVertexSize() const;
    friend std::ostream& operator<<(std::ostream& out, const VertexArray& obj);

private:
    std::string  m_Attribute;
    DataType     m_DataType;
    size_t       m_VertexCount;
    Core::Buffer m_Data;
};

class IndexArray {
public:
    enum struct DataType {
        Int8,
        Int16,
        Int32,
        Int64,
    };

    IndexArray() = default;
    IndexArray(
        const DataType data_type,
        Core::Buffer&& data)
        : m_DataType(data_type),
          m_IndexCount(data.GetDataSize() / GetIndexSize()),
          m_Data(std::move(data)) {}

    inline DataType       GetIndexType() const noexcept { return m_DataType; }
    inline const uint8_t* GetData() const noexcept { return m_Data.GetData(); }
    inline size_t         GetIndexCount() const noexcept { return m_IndexCount; }
    inline size_t         GetDataSize() const noexcept { return m_Data.GetDataSize(); }
    size_t                GetIndexSize() const;

    friend std::ostream& operator<<(std::ostream& out, const IndexArray& obj);

private:
    DataType     m_DataType   = DataType::Int32;
    size_t       m_IndexCount = 0;
    Core::Buffer m_Data;
};

class Mesh : public SceneObject {
public:
    Mesh() = default;

    inline void SetIndexArray(IndexArray array) noexcept { m_IndexArray = std::move(array); }
    inline void AddVertexArray(VertexArray array) noexcept { m_VertexArray.emplace_back(std::move(array)); }
    inline void SetPrimitiveType(PrimitiveType type) noexcept { m_PrimitiveType = type; }
    inline void SetMaterial(std::shared_ptr<Material> material) noexcept { m_MaterialRef = material; }

    std::shared_ptr<Bone> CreateNewBone(std::string name);

    const VertexArray&   GetVertexByName(std::string_view name) const;
    inline size_t        GetVertexArraysCount() const noexcept { return m_VertexArray.size(); }
    inline const auto&   GetVertexArrays() const noexcept { return m_VertexArray; }
    inline const auto&   GetIndexArray() const noexcept { return m_IndexArray; }
    inline PrimitiveType GetPrimitiveType() const noexcept { return m_PrimitiveType; }

    inline const auto& GetMaterial() const noexcept { return m_MaterialRef; }
    bool               HasBones() const noexcept { return !m_Bones.empty(); }

    friend std::ostream& operator<<(std::ostream& out, const Mesh& obj);

private:
    std::vector<VertexArray>           m_VertexArray;
    IndexArray                         m_IndexArray;
    PrimitiveType                      m_PrimitiveType = PrimitiveType::TriangleList;
    std::weak_ptr<Material>            m_MaterialRef;
    std::vector<std::shared_ptr<Bone>> m_Bones;
};

}  // namespace Hitagi::Asset