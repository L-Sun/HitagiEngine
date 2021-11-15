#pragma once
#include "../Format.hpp"
#include "D3Dpch.hpp"

#include "Primitive.hpp"

namespace Hitagi::Graphics::backend::DX12 {
// Static method
inline DXGI_FORMAT ToDxgiFormat(Graphics::Format format) noexcept { return static_cast<DXGI_FORMAT>(format); }

inline Graphics::Format ToFormat(DXGI_FORMAT format) noexcept { return static_cast<Graphics::Format>(format); }

inline D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDxTopologyType(PrimitiveType type) noexcept {
    switch (type) {
        case PrimitiveType::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveType::LineList:
        case PrimitiveType::LineStrip:
        case PrimitiveType::LineListAdjacency:
        case PrimitiveType::LineStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveType::TriangleList:
        case PrimitiveType::TriangleStrip:
        case PrimitiveType::TriangleListAdjacency:
        case PrimitiveType::TriangleStripAdjacency:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}

inline D3D12_PRIMITIVE_TOPOLOGY ToDxTopology(PrimitiveType type) noexcept {
    switch (type) {
        case PrimitiveType::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveType::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveType::LineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveType::LineListAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
        case PrimitiveType::LineStripAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
        case PrimitiveType::TriangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveType::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveType::TriangleListAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case PrimitiveType::TriangleStripAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
    }
}

}  // namespace Hitagi::Graphics::backend::DX12