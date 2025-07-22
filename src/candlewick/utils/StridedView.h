#pragma once

#include <iterator>
#include <span>
#include <type_traits>
#include <stdexcept>

namespace candlewick {

/// \brief A strided view to data, allowing for type-erased data.
///
/// The stride is specified in terms of bytes instead of \c T.
/// \tparam T Stored data type.
template <typename T> class strided_view {
  using erased_type =
      std::conditional_t<std::is_const_v<T>, const char, char> *;

  [[nodiscard]] erased_type erased_ptr() const {
    return reinterpret_cast<erased_type>(m_data);
  }

public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = element_type &;
  using const_reference = const element_type &;

  class iterator final {
    pointer _ptr;
    size_type _stride;

  public:
    using iterator_concept = std::forward_iterator_tag;
    using value_type = strided_view::value_type;
    using difference_type = strided_view::difference_type;

    iterator() : _ptr(nullptr), _stride(0) {}

    iterator(pointer ptr, size_type stride) : _ptr(ptr), _stride(stride) {}

    reference operator*() const { return *_ptr; }
    pointer operator->() const { return _ptr; }

    iterator &operator++() {
      auto p = reinterpret_cast<erased_type>(_ptr) + _stride;
      _ptr = reinterpret_cast<pointer>(p);
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    auto operator<=>(const iterator &) const noexcept = default;
  };
  static_assert(std::forward_iterator<iterator>);

  constexpr strided_view() noexcept : m_data(nullptr), m_size(0), m_stride(0) {}

  /// \brief Build a view from an iterator given the size and stride.
  template <std::random_access_iterator It>
  strided_view(It first, size_type count, size_type stride_bytes) noexcept
      : m_data(std::to_address(first)), m_size(count), m_stride(stride_bytes) {}

  /// \brief Build a view from an iterator and given element count.
  /// The stride is assumed to be \c sizeof(T), i.e. the data is contiguous.
  template <std::random_access_iterator It>
  strided_view(It first, size_type count) noexcept
      : strided_view(first, count, sizeof(T)) {}

  template <size_t extent>
  strided_view(std::span<element_type, extent> other,
               size_type stride_bytes) noexcept
      : m_data(other.data()), m_size(other.size()), m_stride(stride_bytes) {}

  template <size_t extent>
  strided_view(std::span<element_type, extent> other) noexcept
      : strided_view(other, sizeof(element_type)) {}

  template <size_t array_extent>
  strided_view(std::type_identity_t<element_type> (&arr)[array_extent],
               size_t stride_bytes = sizeof(element_type)) noexcept
      : strided_view(static_cast<pointer>(arr), array_extent, stride_bytes) {}

  ~strided_view() noexcept = default;

  strided_view &operator=(const strided_view &) noexcept = default;

  iterator begin() const { return iterator{m_data, m_stride}; }

  iterator end() const {
    return iterator{
        reinterpret_cast<pointer>(erased_ptr() + m_stride * max_index()),
        m_stride};
  }

  iterator cbegin() const { return this->begin(); }
  iterator cend() const { return this->end(); }

  /// \brief Size (number of elements) of the view.
  [[nodiscard]] size_type size() const noexcept { return m_size; }

  /// \brief Stride in bytes between two elements of the view.
  [[nodiscard]] size_type stride_bytes() const noexcept { return m_stride; }

  [[nodiscard]] size_t max_index() const {
    size_t stride_in_T = m_stride / sizeof(element_type);
    size_t q = m_size / stride_in_T;
    size_t m = m_size % stride_in_T;
    return q + ((m > 0) ? 1 : 0);
  }

  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  [[nodiscard]] reference front() const noexcept { return *m_data; }

  [[nodiscard]] reference back() const noexcept {
    return *reinterpret_cast<pointer>(erased_ptr() +
                                      m_stride * (max_index() - 1));
  }

  [[nodiscard]] reference operator[](size_type idx) const noexcept {
    return *reinterpret_cast<pointer>(erased_ptr() + m_stride * idx);
  }

  [[nodiscard]] reference at(size_type idx) const {
    if (idx >= max_index())
      throw std::out_of_range("Access out of range.");
    return this->operator[](idx);
  }

  [[nodiscard]] pointer data() const noexcept { return m_data; }

private:
  pointer m_data;     //< Pointer to the first element in the view
  size_type m_size;   //<
  size_type m_stride; //< Stride in  bytes
};

template <typename T> strided_view(T *first, size_t, size_t) -> strided_view<T>;

template <typename T, size_t extent>
strided_view(std::span<T, extent>, size_t) -> strided_view<T>;

template <typename T, size_t arr_extent>
strided_view(T (&)[arr_extent]) -> strided_view<T>;

} // namespace candlewick
