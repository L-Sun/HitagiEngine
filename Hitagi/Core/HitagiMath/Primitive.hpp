#pragma once
#include <iostream>

namespace Hitagi {

enum struct PrimitiveType {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    LineListAdjacency,
    LineStripAdjacency,
    TriangleListAdjacency,
    TriangleStripAdjacency,
};

}  // namespace Hitagi
