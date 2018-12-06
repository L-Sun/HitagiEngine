#include "geommath.hpp"

namespace My {
typedef vec3 RGB;
typedef vec3 YCbCr;

// clang-format off
const mat4 RGB2YCbCr = {
    {0.299f, -0.168736f,       0.5f, 0.0f},
    {0.587f, -0.331264f, -0.418688f, 0.0f},
    {0.114f,       0.5f, -0.081312f, 0.0f},
    {  0.0f,     128.0f,     128.0f, 0.0f}
};
const mat4 YCbCr2RGB = {
    {      1.0f,        1.0f,      1.0f, 0.0f},
    {      0.0f,  -0.344136f,    1.772f, 0.0f},
    {    1.402f,  -0.714136f,      0.0f, 0.0f},
    { -179.456f, 135.458816f, -226.816f, 0.0f}
};
// clang-format on

YCbCr ConvertRGB2YCbCr(const RGB& rgb) {
    vec4 result = vec4(rgb, 0.0f) * RGB2YCbCr;
    return result.xyz;
}
YCbCr ConvertRGB2YCbCr(const float& r, const float& g, const float& b) {
    return ConvertRGB2YCbCr(RGB(r, g, b));
}

RGB ConvertYCbCr2RGB(const YCbCr& ycbcr) {
    vec4 result = vec4(ycbcr, 0.0f) * YCbCr2RGB;
    return result.xyz;
}
RGB ConvertYCbCr2RGB(const float& Y, const float& Cb, const float& Cr) {
    return ConvertYCbCr2RGB(YCbCr(Y, Cb, Cr));
}
}  // namespace My
