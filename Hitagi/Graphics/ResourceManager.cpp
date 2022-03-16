#include "ResourceManager.hpp"

#include <spdlog/spdlog.h>
#include <fmt/format.h>

namespace Hitagi::Graphics {
std::shared_ptr<MeshBuffer> ResourceManager::GetMeshBuffer(const Asset::Mesh& mesh) {
    auto id = mesh.GetGuid();
    if (m_MeshBuffer.count(id) != 0)
        return m_MeshBuffer.at(id);

    auto result = std::make_shared<MeshBuffer>();

    // Create new vertex array
    for (auto&& vertex : mesh.GetVertexArrays()) {
        std::string_view name = vertex.GetAttributeName();
        result->vertices.emplace(
            name,                         // attribut name
            m_Driver.CreateVertexBuffer(  // backend vertex buffer
                fmt::format("{}-{}", name, id.str()),
                vertex.GetVertexCount(),
                vertex.GetVertexSize(),
                vertex.GetData()));
    }
    // Create Index array
    auto& index_array = mesh.GetIndexArray();
    result->indices   = m_Driver.CreateIndexBuffer(
          fmt::format("index-{}", id.str()),
          index_array.GetIndexCount(),
          index_array.GetIndexSize(),
          index_array.GetData());

    result->primitive = mesh.GetPrimitiveType();

    m_MeshBuffer.emplace(id, result);

    return result;
}

std::shared_ptr<TextureBuffer> ResourceManager::GetTextureBuffer(std::shared_ptr<Asset::Image> texture) {
    if (!texture) return GetDefaultTextureBuffer(Format::R8G8B8A8_UNORM);

    auto id = texture->GetGuid();
    if (m_TextureBuffer.count(id) != 0)
        return m_TextureBuffer.at(id);

    Format format;
    if (auto bit_count = texture->GetBitcount(); bit_count == 8)
        format = Format::R8_UNORM;
    else if (bit_count == 16)
        format = Format::R8G8_UNORM;
    else if (bit_count == 24)
        format = Format::R8G8B8A8_UNORM;
    else if (bit_count == 32)
        format = Format::R8G8B8A8_UNORM;
    else
        format = Format::UNKNOWN;

    // Create new texture buffer
    TextureBuffer::Description desc = {};
    desc.format                     = format;
    desc.width                      = texture->GetWidth();
    desc.height                     = texture->GetHeight();
    desc.pitch                      = texture->GetPitch();
    desc.initial_data               = texture->GetData();
    desc.initial_data_size          = texture->GetDataSize();

    m_TextureBuffer.emplace(id, m_Driver.CreateTextureBuffer(id.str(), desc));
    return m_TextureBuffer.at(id);
}

std::shared_ptr<TextureBuffer> ResourceManager::GetDefaultTextureBuffer(Format format) {
    if (m_DefaultTextureBuffer.count(format) != 0)
        return m_DefaultTextureBuffer.at(format);

    TextureBuffer::Description desc = {};
    desc.format                     = format;
    desc.width                      = 100;
    desc.height                     = 100;
    desc.pitch                      = get_format_bit_size(format);
    desc.sample_count               = 1;
    desc.sample_quality             = 0;
    m_DefaultTextureBuffer.emplace(format, m_Driver.CreateTextureBuffer("Default Texture", desc));
    return m_DefaultTextureBuffer.at(format);
}

std::shared_ptr<Sampler> ResourceManager::GetSampler(std::string_view name) {
    const std::string search_name(name);
    if (m_Samplers.count(search_name) != 0) return m_Samplers.at(search_name);

    // TODO should throw error
    auto&& [iter, success] = m_Samplers.emplace(search_name, m_Driver.CreateSampler(name, {}));
    return iter->second;
}

void ResourceManager::MakeImGuiMesh(ImDrawData* data, ImGuiMeshBuilder&& builder) {
    if (data->DisplaySize.x <= 0.0f || data->DisplaySize.y <= 0.0f || data->TotalIdxCount == 0 || data->TotalIdxCount == 0)
        return;

    if (m_ImGuiVertexBuffer == nullptr || m_ImGuiVertexBuffer->vertex_count < data->TotalVtxCount) {
        m_ImGuiVertexBuffer = m_Driver.CreateVertexBuffer("ImGui Vertex", data->TotalVtxCount, sizeof(ImDrawVert));
    }
    if (m_ImGuiIndexBuffer == nullptr || m_ImGuiIndexBuffer->index_count < data->TotalIdxCount) {
        m_ImGuiIndexBuffer = m_Driver.CreateIndexBuffer("ImGui Index", data->TotalIdxCount, sizeof(ImDrawIdx));
    }

    auto cmd_context = m_Driver.GetGraphicsCommandContext();

    size_t total_vertex_offset = 0;
    size_t total_index_offset  = 0;
    for (size_t i = 0; i < data->CmdListsCount; i++) {
        const auto cmd_list = data->CmdLists[i];

        // Update vertex and index
        {
            cmd_context->UpdateBuffer(
                m_ImGuiVertexBuffer,
                total_vertex_offset * sizeof(ImDrawVert),
                reinterpret_cast<const uint8_t*>(cmd_list->VtxBuffer.Data),
                cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));

            cmd_context->UpdateBuffer(
                m_ImGuiIndexBuffer,
                total_index_offset * sizeof(ImDrawIdx),
                reinterpret_cast<const uint8_t*>(cmd_list->IdxBuffer.Data),
                cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        }

        for (const auto& cmd : cmd_list->CmdBuffer) {
            vec2f clip_min(cmd.ClipRect.x - data->DisplayPos.x, cmd.ClipRect.y - data->DisplayPos.y);
            vec2f clip_max(cmd.ClipRect.z - data->DisplayPos.x, cmd.ClipRect.w - data->DisplayPos.y);
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            auto mesh = std::make_shared<MeshBuffer>();
            mesh->vertices.emplace("POSITION", m_ImGuiVertexBuffer);
            mesh->vertices.emplace("TEXCOORD", m_ImGuiVertexBuffer);
            mesh->vertices.emplace("COLOR", m_ImGuiVertexBuffer);
            mesh->indices       = m_ImGuiIndexBuffer;
            mesh->vertex_offset = total_vertex_offset + cmd.VtxOffset;
            mesh->index_offset  = total_index_offset + cmd.IdxOffset;
            mesh->index_count   = cmd.ElemCount;
            mesh->primitive     = PrimitiveType::TriangleList;

            builder(mesh, cmd_list, cmd);
        }

        total_vertex_offset += cmd_list->VtxBuffer.Size;
        total_index_offset += cmd_list->IdxBuffer.Size;
    }

    cmd_context->Finish(true);
}
}  // namespace Hitagi::Graphics