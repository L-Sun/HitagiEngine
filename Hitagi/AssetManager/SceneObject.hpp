#pragma once
#include "HitagiMath.hpp"
#include "Image.hpp"
#include "Buffer.hpp"

#include <crossguid/guid.hpp>

#include <filesystem>
#include <variant>

namespace Hitagi::Asset {
enum struct SceneObjectType {
    Mesh,
    Material,
    Texture,
    Light,
    Camera,
    Animator,
    Clip,
    Geometry,
    IndexArray,
    VertexArray,
};

enum struct SceneObjectCollisionType {
    None,
    Sphere,
    BOx,
    Cylinder,
    Capsule,
    Cone,
    MultiSphere,
    ConvexHull,
    ConvexMesh,
    BvhMesh,
    HeightField,
    Plane,
};

enum struct VertexDataType {
    Float1,
    Float2,
    Float3,
    Float4,
    Double1,
    Double2,
    Double3,
    Double4,
};

enum struct IndexDataType {
    Int8,
    Int16,
    Int32,
    Int64,
};

class SceneObject {
protected:
    SceneObjectType m_Type;
    std::string     m_Name;
    xg::Guid        m_Guid;

    SceneObject(SceneObjectType type, std::string name = "") : m_Type(type), m_Name(std::move(name)), m_Guid(xg::newGuid()) {}
    SceneObject(const SceneObject& obj) : m_Type(obj.m_Type), m_Name(obj.m_Name), m_Guid(xg::newGuid()) {}
    SceneObject& operator=(const SceneObject& rhs) {
        if (this != &rhs) {
            m_Type = rhs.m_Type;
            m_Name = rhs.m_Name;
        }
        return *this;
    }

    SceneObject(SceneObject&&) = default;
    SceneObject& operator=(SceneObject&&) = default;

public:
    inline const xg::Guid&    GetGuid() const noexcept { return m_Guid; }
    inline void               SetName(std::string name) noexcept { m_Name = std::move(name); }
    inline const std::string& GetName() const noexcept { return m_Name; }

    friend std::ostream& operator<<(std::ostream& out, const SceneObject& obj);
};

class Texture : public SceneObject {
protected:
    uint32_t               m_TexCoordIndex = 0;
    std::filesystem::path  m_TexturePath;
    std::shared_ptr<Image> m_Image = nullptr;

public:
    Texture(const std::filesystem::path& path) : SceneObject(SceneObjectType::Texture, path.filename().string()), m_TexturePath(path) {}
    Texture(std::string name, uint32_t coord_index, std::shared_ptr<Image> image)
        : SceneObject(SceneObjectType::Texture, std::move(name)), m_TexCoordIndex(coord_index), m_Image(std::move(image)) {}
    Texture(Texture&)  = default;
    Texture(Texture&&) = default;

    void                   LoadTexture();
    std::shared_ptr<Image> GetTextureImage() const;
    friend std::ostream&   operator<<(std::ostream& out, const Texture& obj);
};

class Material : public SceneObject {
public:
    template <typename T>
    using Parameter = std::variant<T, std::shared_ptr<Texture>>;

    using Color       = Parameter<vec4f>;
    using Normal      = Parameter<vec3f>;
    using SingleValue = Parameter<float>;

    Material(std::string name) : SceneObject(SceneObjectType::Material, std::move(name)),
                                 m_AmbientColor(vec4f(0.0f)),
                                 m_DiffuseColor(vec4f(1.0f)),
                                 m_Metallic(0.0f),
                                 m_Roughness(0.0f),
                                 m_Normal(vec3f(0.0f, 0.0f, 1.0f)),
                                 m_Specular(vec4f(0.0f)),
                                 m_SpecularPower(1.0f),
                                 m_AmbientOcclusion(1.0f),
                                 m_Opacity(1.0f),
                                 m_Transparency(vec4f(0.0f)),
                                 m_Emission(vec4f(0.0f)){};

    inline const Color&       GetAmbientColor() const noexcept { return m_AmbientColor; }
    inline const Color&       GetDiffuseColor() const noexcept { return m_DiffuseColor; }
    inline const Color&       GetSpecularColor() const noexcept { return m_Specular; }
    inline const SingleValue& GetSpecularPower() const noexcept { return m_SpecularPower; }
    inline const Color&       GetEmission() const noexcept { return m_Emission; }

    void SetColor(std::string_view attrib, const vec4f& color);
    void SetParam(std::string_view attrib, const float param);
    void SetTexture(std::string_view attrib, const std::shared_ptr<Texture>& texture);
    void LoadTextures();

    friend std::ostream& operator<<(std::ostream& out, const Material& obj);

protected:
    Color       m_AmbientColor;
    Color       m_DiffuseColor;
    SingleValue m_Metallic;
    SingleValue m_Roughness;
    Normal      m_Normal;
    Color       m_Specular;
    SingleValue m_SpecularPower;
    SingleValue m_AmbientOcclusion;
    SingleValue m_Opacity;
    Color       m_Transparency;
    Color       m_Emission;
};

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const Material::Parameter<T>& param) {
    if (std::holds_alternative<T>(param))
        return out << std::get<T>(param);
    else
        return out << *std::get<std::shared_ptr<Texture>>(param);
}

class Vertices : public SceneObject {
public:
    Vertices(
        std::string_view attr,
        VertexDataType   data_type,
        Core::Buffer&&   buffer,
        uint32_t         morph_index = 0);

    Vertices(const Vertices&) = default;
    Vertices(Vertices&&)      = default;
    Vertices& operator=(const Vertices&) = default;
    Vertices& operator=(Vertices&&) = default;
    ~Vertices()                     = default;

    const std::string&   GetAttributeName() const;
    VertexDataType       GetDataType() const;
    size_t               GetDataSize() const;
    const uint8_t*       GetData() const;
    size_t               GetVertexCount() const;
    size_t               GetVertexSize() const;
    friend std::ostream& operator<<(std::ostream& out, const Vertices& obj);

private:
    std::string    m_Attribute;
    VertexDataType m_DataType;
    size_t         m_VertexCount;
    Core::Buffer   m_Data;

    uint32_t m_MorphTargetIndex;
};

class Indices : public SceneObject {
public:
    Indices() : SceneObject(SceneObjectType::IndexArray) {}
    Indices(
        const IndexDataType data_type,
        Core::Buffer&&      data,
        const uint32_t      restart_index = 0);

    Indices(const Indices&) = default;
    Indices(Indices&&)      = default;
    Indices& operator=(const Indices&) = default;
    Indices& operator=(Indices&&) = default;
    ~Indices()                    = default;

    const IndexDataType  GetIndexType() const;
    const uint8_t*       GetData() const;
    size_t               GetIndexCount() const;
    size_t               GetDataSize() const;
    size_t               GetIndexSize() const;
    friend std::ostream& operator<<(std::ostream& out, const Indices& obj);

private:
    IndexDataType m_DataType;
    size_t        m_IndexCount = 0;
    Core::Buffer  m_Data;

    size_t m_ResetartIndex = 0;
};

class Mesh : public SceneObject {
protected:
    // Different vector elements correspond to different attributes of
    // vertices, such as the first element is coorespond to the position of
    // vertices, the second element is coorrespond to the normal of
    // vertices, etc.
    std::vector<Vertices>   m_VertexArray;
    Indices                 m_IndexArray;
    std::weak_ptr<Material> m_Material;
    PrimitiveType           m_PrimitiveType;

    bool m_Visible = true;

public:
    Mesh() : SceneObject(SceneObjectType::Mesh) {}

    // Set some things
    void SetIndexArray(Indices&& array);
    void AddVertexArray(Vertices&& array);
    void SetPrimitiveType(PrimitiveType type);
    void SetMaterial(const std::weak_ptr<Material>& material);

    // Get some things
    const Vertices&              GetVertexByName(std::string_view name) const;
    size_t                       GetVertexArraysCount() const;
    const std::vector<Vertices>& GetVertexArrays() const;
    const Indices&               GetIndexArray() const;
    const PrimitiveType&         GetPrimitiveType() const;
    std::weak_ptr<Material>      GetMaterial() const;

    friend std::ostream& operator<<(std::ostream& out, const Mesh& obj);
};

class Geometry : public SceneObject {
protected:
    // LOD | meshes array
    //  0  | meshes array[0] include multiple meshes at 0 LOD
    // ... | ...
    std::vector<std::vector<std::unique_ptr<Mesh>>> m_MeshesLOD;

    bool m_Visible = true;

public:
    Geometry() : SceneObject(SceneObjectType::Geometry) {}
    void                                      SetVisibility(bool visible);
    const bool                                Visible() const;
    void                                      AddMesh(std::unique_ptr<Mesh> mesh, size_t level = 0);
    const std::vector<std::unique_ptr<Mesh>>& GetMeshes(size_t lod = 0) const;
    friend std::ostream&                      operator<<(std::ostream& out, const Geometry& obj);
};

using AttenFunc = std::function<float(float, float)>;

float default_atten_func(float intensity, float distance);

class Light : public SceneObject {
protected:
    Light(const vec4f& color = vec4f(1.0f), float intensity = 100.0f)
        : SceneObject(SceneObjectType::Light),
          m_LightColor(color),
          m_Intensity(intensity),
          m_LightAttenuation(default_atten_func){};

    vec4f       m_LightColor;
    float       m_Intensity;
    AttenFunc   m_LightAttenuation;
    std::string m_TextureRef;

    friend std::ostream& operator<<(std::ostream& out, const Light& obj);

public:
    inline void         SetAttenuation(AttenFunc func) noexcept { m_LightAttenuation = func; }
    inline const vec4f& GetColor() const noexcept { return m_LightColor; }
    inline float        GetIntensity() const noexcept { return m_Intensity; }
};

class PointLight : public Light {
public:
    PointLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f)
        : Light(color, intensity) {}

    friend std::ostream& operator<<(std::ostream& out, const PointLight& obj);
};

class SpotLight : public Light {
protected:
    vec3f m_Direction{};
    float m_InnerConeAngle;
    float m_OuterConeAngle;

public:
    SpotLight(const vec4f& color = vec4f(1.0f), float intensity = 100.0f,
              const vec3f& direction = vec3f(0.0f), float inner_cone_angle = std::numbers::pi / 3.0f,
              float outer_cone_angle = std::numbers::pi / 4.0f)
        : Light(color, intensity), m_InnerConeAngle(inner_cone_angle), m_OuterConeAngle(outer_cone_angle) {}

    friend std::ostream& operator<<(std::ostream& out, const SpotLight& obj);
};

class InfiniteLight : public Light {
public:
    using Light::Light;
    friend std::ostream& operator<<(std::ostream& out, const InfiniteLight& obj);
};

class Camera : public SceneObject {
public:
    Camera(float aspect = 16.0f / 9.0f, float near_clip = 1.0f, float far_clip = 100.0f, float fov = std::numbers::pi / 4)
        : SceneObject(SceneObjectType::Camera),
          m_Aspect(aspect),
          m_NearClipDistance(near_clip),
          m_FarClipDistance(far_clip),
          m_Fov(fov) {}

    void  SetColor(std::string_view attrib, const vec4f& color);
    void  SetParam(std::string_view attrib, float param);
    void  SetTexture(std::string_view attrib, std::string_view texture_name);
    float GetAspect() const;
    float GetNearClipDistance() const;
    float GetFarClipDistance() const;
    float GetFov() const;

    friend std::ostream& operator<<(std::ostream& out, const Camera& obj);

protected:
    float m_Aspect;
    float m_NearClipDistance;
    float m_FarClipDistance;
    float m_Fov;
};

}  // namespace Hitagi::Asset
