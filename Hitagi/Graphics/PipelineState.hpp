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
    std::string           semanticName;
    unsigned              semanticIndex;
    Format                format;
    unsigned              inputSlot;
    size_t                alignedByOffset;
    std::optional<size_t> instanceCount;
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
        unsigned           registerIndex;
        unsigned           space;

        bool operator<(const Parameter& rhs) const {
            // sort priority order: visibility > type > space > registerkkl
            return visibility != rhs.visibility
                       ? static_cast<int>(visibility) < static_cast<int>(rhs.visibility)
                   : type != rhs.type
                       ? static_cast<int>(type) < static_cast<int>(rhs.type)
                   : space != rhs.space
                       ? space < rhs.space
                       : registerIndex < rhs.registerIndex;
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
        unsigned           registerIndex,
        unsigned           space,
        ShaderVisibility   visibility = ShaderVisibility::All);
    // TODO
    RootSignature& AddStaticSampler(
        unsigned             registerIndex,
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
    PipelineState& SetInputLayout(const std::vector<InputLayout>& inputLayout);
    PipelineState& SetRootSignautre(std::shared_ptr<RootSignature> sig);
    PipelineState& SetRenderFormat(Format format);
    PipelineState& SetDepthBufferFormat(Format format);
    void           Create(DriverAPI& driver);

    inline const std::string&              GetName() const noexcept { return m_Name; }
    inline std::shared_ptr<VertexShader>   GetVS() const noexcept { return m_Vs; }
    inline std::shared_ptr<PixelShader>    GetPS() const noexcept { return m_Ps; }
    inline const std::vector<InputLayout>& GetInputLayout() const noexcept { return m_InputLayout; }
    inline std::shared_ptr<RootSignature>  GetRootSignature() const noexcept { return m_RootSignature; }
    inline Format                          GetRenderTargetFormat() const noexcept { return m_RenderFormat; }
    inline Format                          GetDepthBufferFormat() const noexcept { return m_DepthBufferFormat; }

private:
    bool                           m_Created = false;
    std::shared_ptr<VertexShader>  m_Vs;
    std::shared_ptr<PixelShader>   m_Ps;
    std::vector<InputLayout>       m_InputLayout;
    std::shared_ptr<RootSignature> m_RootSignature     = nullptr;
    Format                         m_RenderFormat      = Format::UNKNOWN;
    Format                         m_DepthBufferFormat = Format::UNKNOWN;
};

}  // namespace Hitagi::Graphics