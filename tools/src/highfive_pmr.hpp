#include <highfive/H5Easy.hpp>

#include <memory_resource>
#include "highfive/bits/H5Converter_misc.hpp"

namespace HighFive {
// Fixed-Length strings
// require class specialization templated for the char length
template <>
inline AtomicType<std::pmr::string>::AtomicType() {
    _hid = create_string(H5T_VARIABLE);
}

namespace detail {
template <>
struct inspector<std::pmr::string> : type_helper<std::pmr::string> {
    using hdf5_type = const char*;

    static hdf5_type* data(type& /* val */) {
        throw DataSpaceException("A std::pmr::string cannot be read directly.");
    }

    static const hdf5_type* data(const type& /* val */) {
        throw DataSpaceException("A std::pmr::string cannot be write directly.");
    }

    static void serialize(const type& val, hdf5_type* m) {
        *m = val.c_str();
    }

    static void unserialize(const hdf5_type* vec,
                            const std::vector<size_t>& /* dims */,
                            type& val) {
        val = vec[0];
    }
};

template <typename T>
struct inspector<std::pmr::vector<T>> {
    using type       = std::pmr::vector<T>;
    using value_type = unqualified_t<T>;
    using base_type  = typename inspector<value_type>::base_type;
    using hdf5_type  = typename inspector<value_type>::hdf5_type;

    static constexpr size_t ndim                  = 1;
    static constexpr size_t recursive_ndim        = ndim + inspector<value_type>::recursive_ndim;
    static constexpr bool   is_trivially_copyable = std::is_trivially_copyable<value_type>::value;

    static std::vector<size_t> getDimensions(const type& val) {
        std::vector<size_t> sizes{val.size()};
        if (!val.empty()) {
            auto s = inspector<value_type>::getDimensions(val[0]);
            sizes.insert(sizes.end(), s.begin(), s.end());
        }
        return sizes;
    }

    static size_t getSizeVal(const type& val) {
        return compute_total_size(getDimensions(val));
    }

    static size_t getSize(const std::vector<size_t>& dims) {
        return compute_total_size(dims);
    }

    static void prepare(type& val, const std::vector<size_t>& dims) {
        val.resize(dims[0]);
        std::vector<size_t> next_dims(dims.begin() + 1, dims.end());
        for (auto& e : val) {
            inspector<value_type>::prepare(e, next_dims);
        }
    }

    static hdf5_type* data(type& val) {
        return inspector<value_type>::data(val[0]);
    }

    static const hdf5_type* data(const type& val) {
        return inspector<value_type>::data(val[0]);
    }

    static void serialize(const type& val, hdf5_type* m) {
        size_t subsize = inspector<value_type>::getSizeVal(val[0]);
        for (auto& e : val) {
            inspector<value_type>::serialize(e, m);
            m += subsize;
        }
    }

    static void unserialize(const hdf5_type*           vec_align,
                            const std::vector<size_t>& dims,
                            type&                      val) {
        std::vector<size_t> next_dims(dims.begin() + 1, dims.end());
        size_t              next_size = compute_total_size(next_dims);
        for (size_t i = 0; i < dims[0]; ++i) {
            inspector<value_type>::unserialize(vec_align + i * next_size, next_dims, val[i]);
        }
    }
};
}  // namespace detail
}  // namespace HighFive