//
// /home/lawliet/workspace/cpp/game_engine/Framework/GeomMath/include/Multiplication.h
// (Header automatically generated by the ispc compiler.)
// DO NOT EDIT THIS FILE.
//

#ifndef ISPC__HOME_LAWLIET_WORKSPACE_CPP_GAME_ENGINE_FRAMEWORK_GEOMMATH_INCLUDE_MULTIPLICATION_H
#define ISPC__HOME_LAWLIET_WORKSPACE_CPP_GAME_ENGINE_FRAMEWORK_GEOMMATH_INCLUDE_MULTIPLICATION_H

#include <stdint.h>



#ifdef __cplusplus
namespace ispc { /* namespace */
#endif // __cplusplus

#ifndef __ISPC_ALIGN__
#if defined(__clang__) || !defined(_MSC_VER)
// Clang, GCC, ICC
#define __ISPC_ALIGN__(s) __attribute__((aligned(s)))
#define __ISPC_ALIGNED_STRUCT__(s) struct __ISPC_ALIGN__(s)
#else
// Visual Studio
#define __ISPC_ALIGN__(s) __declspec(align(s))
#define __ISPC_ALIGNED_STRUCT__(s) __ISPC_ALIGN__(s) struct
#endif
#endif


///////////////////////////////////////////////////////////////////////////
// Functions exported from ispc code
///////////////////////////////////////////////////////////////////////////
#if defined(__cplusplus) && (! defined(__ISPC_NO_EXTERN_C) || !__ISPC_NO_EXTERN_C )
extern "C" {
#endif // __cplusplus
    extern void CrossProduct(const float * a, const float * b, float * result);
    extern void DotProduct(const float * a, const float * b, float * result, const size_t count);
    extern void MulByElement(const float * a, const float * b, float * result, const size_t count);
    extern void MulByNum(const float * a, const float b, float * result, const size_t count);
    extern void MulSelfByNum(float * a, const float b, const size_t count);
#if defined(__cplusplus) && (! defined(__ISPC_NO_EXTERN_C) || !__ISPC_NO_EXTERN_C )
} /* end extern C */
#endif // __cplusplus


#ifdef __cplusplus
} /* namespace */
#endif // __cplusplus

#endif // ISPC__HOME_LAWLIET_WORKSPACE_CPP_GAME_ENGINE_FRAMEWORK_GEOMMATH_INCLUDE_MULTIPLICATION_H
