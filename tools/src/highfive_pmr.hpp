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

namespace details {
template <>
struct inspector<std::pmr::string> : type_helper<std::pmr::string> {
    using hdf5_type = const char*;

    static hdf5_type* data(type& /* val */) {
        throw DataSpaceException("A std::string cannot be read directly.");
    }

    static const hdf5_type* data(const type& /* val */) {
        throw DataSpaceException("A std::string cannot be write directly.");
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

    static void unserialize(const hdf5_type*                vec_align,
                            const std::pmr::vector<size_t>& dims,
                            type&                           val) {
        std::pmr::vector<size_t> next_dims(dims.begin() + 1, dims.end());
        size_t                   next_size = compute_total_size(next_dims);
        for (size_t i = 0; i < dims[0]; ++i) {
            inspector<value_type>::unserialize(vec_align + i * next_size, next_dims, val[i]);
        }
    }
};
}  // namespace details

// template <typename T>
// struct data_converter<
//     std::pmr::vector<T>,
//     typename std::enable_if<(
//         std::is_same<T, typename inspector<T>::base_type>::value &&
//         !std::is_same<T, Reference>::value)>::type>
//     : public container_converter<std::pmr::vector<T>> {
//     using container_converter<std::pmr::vector<T>>::container_converter;
// };

// // apply conversion for vectors nested vectors
// template <typename T>
// struct data_converter<std::pmr::vector<T>,
//                       typename std::enable_if<(is_container<T>::value)>::type> {
//     using value_type = typename inspector<T>::base_type;

//     inline data_converter(const DataSpace& space)
//         : _dims(space.getDimensions()) {}

//     inline value_type* transform_read(std::pmr::vector<T>&) {
//         _vec_align.resize(compute_total_size(_dims));
//         return _vec_align.data();
//     }

//     inline const value_type* transform_write(const std::pmr::vector<T>& vec) {
//         _vec_align.reserve(compute_total_size(_dims));
//         vectors_to_single_buffer<T>(vec, _dims, 0, _vec_align);
//         return _vec_align.data();
//     }

//     inline void process_result(std::pmr::vector<T>& vec) const {
//         single_buffer_to_vectors(
//             _vec_align.cbegin(), _vec_align.cend(), _dims, 0, vec);
//     }

//     std::vector<size_t>                                _dims;
//     std::pmr::vector<typename inspector<T>::base_type> _vec_align;
// };

// // apply conversion to scalar pmr string
// template <>
// struct data_converter<std::pmr::string, void> {
//     using value_type = const char*;  // char data is const, mutable pointer

//     inline data_converter(const DataSpace& space) noexcept
//         : _space(space) {}

//     // create a C vector adapted to HDF5
//     // fill last element with NULL to identify end
//     inline value_type* transform_read(std::pmr::string&) noexcept {
//         return &_c_vec;
//     }

//     inline const value_type* transform_write(const std::pmr::string& str) noexcept {
//         _c_vec = str.c_str();
//         return &_c_vec;
//     }

//     inline void process_result(std::pmr::string& str) {
//         assert(_c_vec != nullptr);
//         str = std::pmr::string(_c_vec);

//         if (_c_vec != nullptr) {
//             AtomicType<std::pmr::string> str_type;
//             (void)H5Dvlen_reclaim(str_type.getId(), _space.getId(), H5P_DEFAULT,
//                                   &_c_vec);
//         }
//     }

//     value_type       _c_vec{};
//     const DataSpace& _space;
// };

// template <>
// struct data_converter<std::pmr::vector<std::pmr::string>, void> {
//     using value_type = const char*;

//     inline data_converter(const DataSpace& space) noexcept
//         : _space(space) {}

//     // create a C vector adapted to HDF5
//     // fill last element with NULL to identify end
//     inline value_type* transform_read(std::pmr::vector<std::pmr::string>&) {
//         _c_vec.resize(_space.getDimensions()[0], NULL);
//         return _c_vec.data();
//     }

//     inline const value_type* transform_write(const std::pmr::vector<std::pmr::string>& vec) {
//         _c_vec.resize(vec.size() + 1, NULL);
//         std::transform(vec.begin(), vec.end(), _c_vec.begin(),
//                        [](const std::pmr::string& str) { return str.c_str(); });
//         return _c_vec.data();
//     }

//     inline void process_result(std::pmr::vector<std::pmr::string>& vec) {
//         vec.resize(_c_vec.size());
//         for (size_t i = 0; i < vec.size(); ++i) {
//             vec[i] = std::pmr::string(_c_vec[i]);
//         }

//         if (_c_vec.empty() == false && _c_vec[0] != NULL) {
//             AtomicType<std::pmr::string> str_type;
//             (void)H5Dvlen_reclaim(str_type.getId(), _space.getId(), H5P_DEFAULT,
//                                   &(_c_vec[0]));
//         }
//     }

//     std::pmr::vector<value_type> _c_vec;
//     const DataSpace&             _space;
// };

}  // namespace HighFive