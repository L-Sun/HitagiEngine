#include <highfive/H5Easy.hpp>

#include <memory_resource>

namespace HighFive {
// Fixed-Length strings
// require class specialization templated for the char length
template <>
class AtomicType<std::pmr::string> : public DataType {
public:
    inline AtomicType() : DataType(create_string(H5T_VARIABLE)) {}
};

namespace details {
template <typename T>
struct inspector<std::pmr::vector<T>> {
    using type       = std::pmr::vector<T>;
    using value_type = T;
    using base_type  = typename inspector<value_type>::base_type;

    static constexpr size_t ndim           = 1;
    static constexpr size_t recursive_ndim = ndim + inspector<value_type>::recursive_ndim;

    static std::array<size_t, recursive_ndim> getDimensions(const type& val) {
        std::array<size_t, recursive_ndim> sizes{val.size()};
        size_t                             index = ndim;
        for (const auto& s : inspector<value_type>::getDimensions(val[0])) {
            sizes[index++] = s;
        }
        return sizes;
    }
};

template <typename T>
struct data_converter<
    std::pmr::vector<T>,
    typename std::enable_if<(
        std::is_same<T, typename inspector<T>::base_type>::value &&
        !std::is_same<T, Reference>::value)>::type>
    : public container_converter<std::pmr::vector<T>> {
    using container_converter<std::pmr::vector<T>>::container_converter;
};

}  // namespace details
}  // namespace HighFive