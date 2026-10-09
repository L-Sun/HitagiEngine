module;
#include <D3D12MemAlloc.h>

export module interop.d3d12ma;

export namespace D3D12MA {
using ::D3D12MA::Allocation;
using ::D3D12MA::ALLOCATION_CALLBACKS;
using ::D3D12MA::ALLOCATION_DESC;
using ::D3D12MA::Allocator;
using ::D3D12MA::ALLOCATOR_DESC;
using ::D3D12MA::ALLOCATOR_FLAG_NONE;
using ::D3D12MA::Budget;
using ::D3D12MA::CreateAllocator;
}  // namespace D3D12MA
