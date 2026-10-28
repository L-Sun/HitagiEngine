#include "ispc_math.hpp"
#include "vector.ispc.h"
#include "matrix.ispc.h"

namespace ispc {
void matrix4_mult(const float* a, const float* b, float* out) {
    matrix4_mult_float(a, b, out);
}
void matrix4_mult(const double* a, const double* b, double* out) {
    matrix4_mult_double(a, b, out);
}
void matrix_mult_batch(const float* a, const float* b, float* out, std::int32_t dimension, std::uint64_t count, bool broadcast_lhs) {
    matrix_mult_batch_float(a, b, out, dimension, count, broadcast_lhs);
}
void matrix_mult_batch(const double* a, const double* b, double* out, std::int32_t dimension, std::uint64_t count, bool broadcast_lhs) {
    matrix_mult_batch_double(a, b, out, dimension, count, broadcast_lhs);
}
// float
void vector_add_assign(float* a, const float* b, const int32_t size) {
    vector_add_assign_float(a, b, size);
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
void vector_mult_assign(float* a, const float b, const int32_t size) {
    vector_mult_assign_float(a, b, size);
}
void vector_mult(const float* a, const float b, float* out, const int32_t size) {
    vector_mult_float(a, b, out, size);
}
void vector_mult_vector(const float* a, const float* b, float* out, const int32_t size) {
    vector_mult_vector_float(a, b, out, size);
}
void vector_sub_assign(float* a, const float* b, const int32_t size) {
    vector_sub_assign_float(a, b, size);
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
void vector_add_assign(double* a, const double* b, const int32_t size) {
    vector_add_assign_double(a, b, size);
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
void vector_mult_assign(double* a, const double b, const int32_t size) {
    vector_mult_assign_double(a, b, size);
}
void vector_mult(const double* a, const double b, double* out, const int32_t size) {
    vector_mult_double(a, b, out, size);
}
void vector_mult_vector(const double* a, const double* b, double* out, const int32_t size) {
    vector_mult_vector_double(a, b, out, size);
}
void vector_sub_assign(double* a, const double* b, const int32_t size) {
    vector_sub_assign_double(a, b, size);
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
