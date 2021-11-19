#pragma once
#include "Format.hpp"
#include "ShaderManager.hpp"
#include "Resource.hpp"

#include <string>
#include <optional>
#include <cassert>
#include <array>
#include <set>

namespace Hitagi::Graphics {
class DriverAPI;

struct InputLayout {
    std::string           semantic_name;
    unsigned              semantic_index;
    Format                format;
    unsigned              input_slot;
    size_t                aligned_by_offset;
    std::optional<size_t> instance_count;
};

enum struct ShaderVariableType {
    CBV,
    SRV,
    UAV,
    Sampler,
    Num_Type
};
enum struct ShaderVisibility {
    All,
    Vertex,
    Hull,
    Domain,
    Geometry,
    Pixel,
    Num_Visibility
};

class RootSignature : public Resource {
    struct Parameter {
        std::string        name;
        ShaderVisibility   visibility;
        ShaderVariableType type;
        unsigned           register_index;
        unsigned           space;

        auto operator<=>(const Parameter& rhs) const {
            if (auto cmp = visibility <=> rhs.visibility; cmp != 0)
                return cmp;
            if (auto cmp = type <=> rhs.type; cmp != 0)
                return cmp;
            if (auto cmp = space <=> rhs.space; cmp != 0)
                return cmp;
            return register_index <=> rhs.register_index;
        }
    };

public:
    RootSignature(std::string_view name) : Resource(name, nullptr) {}
    RootSignature(const RootSignature&) = delete;
    RootSignature& operator=(const RootSignature&) = delete;
    RootSignature(RootSignature&&);
    RootSignature& operator=(RootSignature&&);

    RootSignature& Add(
        std::string_view   name,
        ShaderVariableType type,
        unsigned           register_index,
        unsigned           space,
        ShaderVisibility   visibility = ShaderVisibility::All);
    // TODO Sampler
    RootSignature& AddStaticSampler(
        unsigned             register_index,
        Sampler::Description desc,
        ShaderVisibility     visibility = ShaderVisibility::All);
    RootSignature& Create(DriverAPI& driver);

    inline auto& GetParametes() const noexcept { return m_ParameterTable; }

    operator bool() const noexcept { return m_Created; }

protected:
    bool                m_Created = false;
    std::set<Parameter> m_ParameterTable;
};

class PipelineState : public Resource {
public:
    PipelineState(std::string_view name) : Resource(name, nullptr) {}

    PipelineState& SetVertexShader(std::shared_ptr<VertexShader> vs);
    PipelineState& SetPixelShader(std::shared_ptr<PixelShader> ps);
    PipelineState& SetInputLayout(const std::vector<InputLayout>& input_layout);
    PipelineState& SetRootSignautre(std::shared_ptr<RootSignature> sig);
    PipelineState& SetRenderFormat(Format format);
    PipelineState& SetDepthBufferFormat(Format format);
    PipelineState& SetPrimitiveType(PrimitiveType type);
    PipelineState& SetFrontCounterClockwise(bool value);
    void           Create(DriverAPI& driver);

    inline const std::string&              GetName() const noexcept { return m_Name; }
    inline std::shared_ptr<VertexShader>   GetVS() const noexcept { return m_Vs; }
    inline std::shared_ptr<PixelShader>    GetPS() const noexcept { return m_Ps; }
    inline const std::vector<InputLayout>& GetInputLayout() const noexcept { return m_InputLayout; }
    inline std::shared_ptr<RootSignature>  GetRootSignature() const noexcept { return m_RootSignature; }
    inline Format                          GetRenderTargetFormat() const noexcept { return m_RenderFormat; }
    inline Format                          GetDepthBufferFormat() const noexcept { return m_DepthBufferFormat; }
    inline PrimitiveType                   GetPrimitiveType() const noexcept { return m_PrimitiveType; }
    inline bool                            IsFontCounterClockwise() const noexcept { return m_FrontCounterClockwise; }

private:
    bool                           m_Created = false;
    std::shared_ptr<VertexShader>  m_Vs;
    std::shared_ptr<PixelShader>   m_Ps;
    std::vector<InputLayout>       m_InputLayout;
    bool                           m_FrontCounterClockwise = true;
    std::shared_ptr<RootSignature> m_RootSignature         = nullptr;
    Format                         m_RenderFormat          = Format::UNKNOWN;
    Format                         m_DepthBufferFormat     = Format::UNKNOWN;
    PrimitiveType                  m_PrimitiveType         = PrimitiveType::TriangleList;
};

}  // namespace Hitagi::Graphics