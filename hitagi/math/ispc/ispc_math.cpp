#pragma once
#include "ispc_math.hpp"
#include "vector_ispc.h"

namespace ispc {
// float
void vector_add_assgin(float* a, const float* b, const int32_t size) {
    vector_add_assgin_float(a, b, size);
}
void vector_add(const float* a, const float* b, float* out, const int32_t size) {
    vector_add_float(a, b, out, size);
}
void vector_div_assign(float* a, const float b, const int32_t size) {
    vector_div_assign_float(a, b, size);
}
void vector_div(const float* a, const float b, float* out, const int32_t size) {
    vector_div_float(a, b, out, size);
}
void vector_div_vector(const float* a, const float* b, float* out, const int32_t size) {
    vector_div_vector_float(a, b, out, size);
}
float vector_dot(const float* a, const float* b, const int32_t size) {
    return vector_dot_float(a, b, size);
}
void vector_mult_assgin(float* a, const float b, const int32_t size) {
    vector_mult_assgin_float(a, b, size);
}
void vector_mult(const float* a, const float b, float* out, const int32_t size) {
    vector_mult_float(a, b, out, size);
}
void vector_mult_vector(const float* a, const float* b, float* out, const int32_t size) {
    vector_mult_vector_float(a, b, out, size);
}
void vector_sub_assgin(float* a, const float* b, const int32_t size) {
    vector_sub_assgin_float(a, b, size);
}
void vector_sub(const float* a, const float* b, float* out, const int32_t size) {
    vector_sub_float(a, b, out, size);
}
void zero(float* data, int32_t size) {
    zero_float(data, size);
}
void vector_inverse(const float* data, float* out, const int32_t size) {
    vector_inverse_float(data, out, size);
}

// double
void vector_add_assgin(double* a, const double* b, const int32_t size) {
    vector_add_assgin_double(a, b, size);
}
void vector_add(const double* a, const double* b, double* out, const int32_t size) {
    vector_add_double(a, b, out, size);
}
void vector_div_assign(double* a, const double b, const int32_t size) {
    vector_div_assign_double(a, b, size);
}
void vector_div(const double* a, const double b, double* out, const int32_t size) {
    vector_div_double(a, b, out, size);
}
void vector_div_vector(const double* a, const double* b, double* out, const int32_t size) {
    vector_div_vector_double(a, b, out, size);
}
double vector_dot(const double* a, const double* b, const int32_t size) {
    return vector_dot_double(a, b, size);
}
void vector_mult_assgin(double* a, const double b, const int32_t size) {
    vector_mult_assgin_double(a, b, size);
}
void vector_mult(const double* a, const double b, double* out, const int32_t size) {
    vector_mult_double(a, b, out, size);
}
void vector_mult_vector(const double* a, const double* b, double* out, const int32_t size) {
    vector_mult_vector_double(a, b, out, size);
}
void vector_sub_assgin(double* a, const double* b, const int32_t size) {
    vector_sub_assgin_double(a, b, size);
}
void vector_sub(const double* a, const double* b, double* out, const int32_t size) {
    vector_sub_double(a, b, out, size);
}
void zero(double* data, int32_t size) {
    zero_double(data, size);
}
void vector_inverse(const double* data, double* out, const int32_t size) {
    vector_inverse_double(data, out, size);
}
};  // namespace ispc