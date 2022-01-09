#pragma once
#include "IRuntimeModule.hpp"
#include "Allocator.hpp"

namespace Hitagi::Core {
class MemoryManager : public IRuntimeModule {
public:
    template <typename T, typename... Arguments>
    T* New(Arguments... parameters) {
        return new (Allocator(sizeof(T))) T(parameters...);
    }

    template <typename T>
    void Delete(T* p) {
        reinterpret_cast<T*>(p)->~T();
        Free(p, sizeof(T));
    }

    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    [[nodiscard]] void* Allocate(size_t size);
    [[nodiscard]] void* Allocate(size_t size, size_t alignment);
    void                Free(void* p, size_t size);

private:
    inline static size_t*    sm_BlockSizeLookup = nullptr;
    inline static Allocator* sm_Allocators      = nullptr;
    static Allocator*        LookUpAllocator(size_t size);
    bool                     m_Initialized = false;
};

}  // namespace Hitagi::Core
namespace Hitagi {
extern std::unique_ptr<Core::MemoryManager> g_MemoryManager;
}