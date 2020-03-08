#include <iostream>
#include <memory_resource>
#include <algorithm>
#include <vector>

using namespace std;

class Hitagi_memory_resource : public std::pmr::memory_resource {
public:
    ~Hitagi_memory_resource() override {}

    void* allocate();
    void  deallocate(void* p, size_t bytes,
                     size_t alignment = alignof(std::max_align_t));
    bool  is_equal(const Hitagi_memory_resource& other) const;

protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        if (m_nFree < bytes) return nullptr;
        void* ret = static_cast<void*>(m_pFree);
        m_pFree += bytes;
        m_nFree -= bytes;
        m_nAllocated += bytes;
        m_blocks.push_back(alloc_rec{ret, bytes, alignment});
        return ret;
    }
    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        auto i = std::find_if(m_blocks.begin(), m_blocks.end(),
                              [p](alloc_rec& r) { return r.ptr == p; });
        if (i == m_blocks.end())
            throw std::invalid_argument("deallocate: Invalid pointer.");
        else if (i->bytes != bytes)
            throw std::invalid_argument("deallocate: size mismatch");
        else if (i->alignment != alignment)
            throw std::invalid_argument("deallocate: Alignment mismatch");

        m_ndeallocated += bytes;
        m_blocks.erase(i);
    }
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

public:
    struct alloc_rec {
        void*  ptr;
        size_t bytes;
        size_t alignment;
    };

    char                   m_buffer[128];
    char*                  m_pFree        = m_buffer;
    size_t                 m_nFree        = 128;
    size_t                 m_nAllocated   = 0;
    size_t                 m_ndeallocated = 0;
    pmr::vector<alloc_rec> m_blocks;
};

int main(int argc, char const* argv[]) {
    char buffer[128];

    Hitagi_memory_resource ms;

    pmr::monotonic_buffer_resource mbr(buffer, 128);

    pmr::vector<int> vec(&mbr);

    for (size_t i = 0; i < 15; i++) {
        cout << "-------" << i << "-------" << endl;
        vec.push_back(i);
    }

    return 0;
}
