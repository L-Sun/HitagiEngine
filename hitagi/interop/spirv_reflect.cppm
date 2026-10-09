module;
#include <spirv_reflect.h>

export module interop.spirv_reflect;

export {
    using ::SpvReflectDescriptorType;
    using ::SpvReflectFormat;
    using ::SpvReflectInterfaceVariable;
    using ::SpvReflectShaderStageFlagBits;
    using enum SpvReflectDescriptorType;
    using enum SpvReflectFormat;
    using enum SpvReflectShaderStageFlagBits;
}
