#pragma once
#include <concepts>
#include <cstdint>

template <typename T>
concept IspcAccelerable =
    std::same_as<float, T> || std::same_as<double, T>;

namespace ispc {
// float
void  vector_add_assign(float* a, const float* b, const int32_t size);
void  vector_add(const float* a, const float* b, float* out, const int32_t size);
void  vector_div_assign(float* a, const float b, const int32_t size);
void  vector_div(const float* a, const float b, float* out, const int32_t size);
void  vector_div_vector(const float* a, const float* b, float* out, const int32_t size);
float vector_dot(const float* a, const float* b, const int32_t size);
void  vector_mult_assign(float* a, const float b, const int32_t size);
void  vector_mult(const float* a, const float b, float* out, const int32_t size);
void  vector_mult_vector(const float* a, const float* b, float* out, const int32_t size);
void  vector_sub_assign(float* a, const float* b, const int32_t size);
void  vector_sub(const float* a, const float* b, float* out, const int32_t size);
void  zero(float* data, int32_t size);
void  vector_inverse(const float* data, float* out, const int32_t size);

// double
void   vector_add_assign(double* a, const double* b, const int32_t size);
void   vector_add(const double* a, const double* b, double* out, const int32_t size);
void   vector_div_assign(double* a, const double b, const int32_t size);
void   vector_div(const double* a, const double b, double* out, const int32_t size);
void   vector_div_vector(const double* a, const double* b, double* out, const int32_t size);
double vector_dot(const double* a, const double* b, const int32_t size);
void   vector_mult_assign(double* a, const double b, const int32_t size);
void   vector_mult(const double* a, const double b, double* out, const int32_t size);
void   vector_mult_vector(const double* a, const double* b, double* out, const int32_t size);
void   vector_sub_assign(double* a, const double* b, const int32_t size);
void   vector_sub(const double* a, const double* b, double* out, const int32_t size);
void   zero(double* data, int32_t size);
void   vector_inverse(const double* data, double* out, const int32_t size);
};  // namespace ispc