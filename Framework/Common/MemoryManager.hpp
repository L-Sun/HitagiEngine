#pragma once
#include "IRuntimeModule.hpp"
#include "Allocator.hpp"

namespace Hitagi {
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

    virtual ~MemoryManager() {}
    virtual int  Initialize();
    virtual void Finalize();
    virtual void Tick();

    void* Allocate(size_t size);
    void* Allocate(size_t size, size_t alignment);
    void  Free(void* p, size_t size);

private:
    static size_t*    m_BlockSizeLookup;
    static Allocator* m_Allocators;
    static Allocator* LookUpAllocator(size_t size);
    bool              m_Initialized = false;
};

extern std::unique_ptr<MemoryManager> g_MemoryManager;
}  // namespace Hitagi
