#include <iostream>
#include <memory_resource>
#include <algorithm>
#include <vector>
#include <array>

using namespace std;

class hitagiMemoryResource : public std::pmr::memory_resource {
public:
    ~hitagiMemoryResource() override = default;

    void* allocate();
    void  deallocate(void* p, size_t bytes,
                     size_t alignment = alignof(std::max_align_t));
    bool  is_equal(const hitagiMemoryResource& other) const;

protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        if (m_n_free < bytes) return nullptr;
        void* ret = static_cast<void*>(m_p_free);
        m_p_free += bytes;
        m_n_free -= bytes;
        m_n_allocated += bytes;
        m_blocks.push_back(AllocRec{ret, bytes, alignment});
        return ret;
    }
    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        auto i = std::find_if(m_blocks.begin(), m_blocks.end(),
                              [p](AllocRec& r) { return r.ptr == p; });
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
    struct AllocRec {
        void*  ptr;
        size_t bytes;
        size_t alignment;
    };

    std::array<char, 128> m_buffer{};
    char*                 m_p_free       = m_buffer.data();
    size_t                m_n_free       = 128;
    size_t                m_n_allocated  = 0;
    size_t                m_ndeallocated = 0;
    pmr::vector<AllocRec> m_blocks;
};

int main(int argc, char const* argv[]) {
    char buffer[128];

    hitagiMemoryResource ms;

    pmr::monotonic_buffer_resource mbr(buffer, 128);

    pmr::vector<int> vec(&mbr);

    for (size_t i = 0; i < 15; i++) {
        cout << "-------" << i << "-------" << endl;
        vec.push_back(i);
    }

    return 0;
}
