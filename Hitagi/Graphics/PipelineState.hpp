#pragma once
#include "Format.hpp"
#include "ShaderManager.hpp"

#include <string>
#include <optional>
#include <cassert>
#include <array>
#include <set>

namespace Hitagi::Graphics {

namespace backend {
class DriverAPI;
}

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

class RootSignature {
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
    RootSignature();
    RootSignature(const RootSignature&);
    RootSignature(RootSignature&&);
    RootSignature& operator=(const RootSignature&) = delete;
    RootSignature& operator=(RootSignature&&) = delete;
    ~RootSignature();

    RootSignature& Add(
        std::string_view   name,
        ShaderVariableType type,
        unsigned           registerIndex,
        unsigned           space,
        ShaderVisibility   visibility = ShaderVisibility::All);
    void Create(backend::DriverAPI& driver);

    size_t Id() const noexcept { return m_Id; }
    auto&  GetParametes() const noexcept { return m_ParameterTable; }

    operator bool() const noexcept { return m_Created; }

private:
    static size_t GetNewId();

    backend::DriverAPI* m_Driver  = nullptr;
    bool                m_Created = false;
    size_t              m_Id;
    std::set<Parameter> m_ParameterTable;
};

class PipelineState {
public:
    PipelineState(std::string_view name) : m_Name(name) {
        assert(m_PSOName.count(m_Name) == 0 && "there is a pso with the same name.");
        m_PSOName.emplace(m_Name);
    }
    ~PipelineState();

    PipelineState& SetVertexShader(std::shared_ptr<VertexShader> vs);
    PipelineState& SetPixelShader(std::shared_ptr<PixelShader> ps);
    PipelineState& SetInputLayout(const std::vector<InputLayout>& inputLayout);
    PipelineState& SetRootSignautre(std::shared_ptr<RootSignature> sig);
    PipelineState& SetRenderFormat(Format format);
    PipelineState& SetDepthBufferFormat(Format format);
    void           Create(backend::DriverAPI& driver);

    const std::string&              GetName() const noexcept { return m_Name; }
    std::shared_ptr<VertexShader>   GetVS() const noexcept { return m_Vs; }
    std::shared_ptr<PixelShader>    GetPS() const noexcept { return m_Ps; }
    const std::vector<InputLayout>& GetInputLayout() const noexcept { return m_InputLayout; }
    std::shared_ptr<RootSignature>  GetRootSignature() const noexcept { return m_RootSignature; }
    Format                          GetRenderTargetFormat() const noexcept { return m_RenderFormat; }
    Format                          GetDepthBufferFormat() const noexcept { return m_DepthBufferFormat; }

private:
    static std::set<std::string> m_PSOName;

    backend::DriverAPI*            m_Driver;
    bool                           m_Created = false;
    std::string                    m_Name;
    std::shared_ptr<VertexShader>  m_Vs;
    std::shared_ptr<PixelShader>   m_Ps;
    std::vector<InputLayout>       m_InputLayout;
    std::shared_ptr<RootSignature> m_RootSignature     = nullptr;
    Format                         m_RenderFormat      = Format::UNKNOWN;
    Format                         m_DepthBufferFormat = Format::UNKNOWN;
};

}  // namespace Hitagi::Graphics