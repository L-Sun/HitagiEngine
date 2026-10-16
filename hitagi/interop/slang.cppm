module;
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

export module interop.slang;

export namespace hitagi::interop {
namespace slang {
using ::slang::CompilerOptionEntry;
using ::slang::CompilerOptionName;
using ::slang::CompilerOptionValueKind;
using ::slang::createGlobalSession;
using ::slang::IBlob;
using ::slang::IComponentType;
using ::slang::IEntryPoint;
using ::slang::IGlobalSession;
using ::slang::IModule;
using ::slang::ISession;
using ::slang::ParameterCategory;
using ::slang::PreprocessorMacroDesc;
using ::slang::SessionDesc;
using ::slang::SpecializationArg;
using ::slang::TargetDesc;
using ::slang::TypeReflection;
using ::slang::VariableLayoutReflection;
}  // namespace slang
template <typename T>
using SlangPtr = ::Slang::ComPtr<T>;
inline bool           SlangFailed(SlangResult result) noexcept { return SLANG_FAILED(result); }
inline constexpr auto slang_dxil      = SLANG_DXIL;
inline constexpr auto slang_spirv     = SLANG_SPIRV;
inline constexpr auto slang_row_major = SLANG_MATRIX_LAYOUT_ROW_MAJOR;
inline constexpr auto slang_vertex    = SLANG_STAGE_VERTEX;
inline constexpr auto slang_fragment  = SLANG_STAGE_FRAGMENT;
inline constexpr auto slang_geometry  = SLANG_STAGE_GEOMETRY;
inline constexpr auto slang_compute   = SLANG_STAGE_COMPUTE;
}  // namespace hitagi::interop
