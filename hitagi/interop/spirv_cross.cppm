module;
#include <spirv_cross/spirv_msl.hpp>

export module interop.spirv_cross;
export namespace hitagi::interop::cross {
using ::spirv_cross::CompilerMSL;
using ::spirv_cross::MSLResourceBinding;
using ::spirv_cross::SPIRType;
using ::spv::DecorationBinding;
using ::spv::Dim1D;
}
