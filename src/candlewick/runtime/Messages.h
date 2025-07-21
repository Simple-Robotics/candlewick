#pragma once

#include <vector>
#include <string>

#include <zmq.hpp>
#include <msgpack.hpp>

#include <Eigen/Core>

namespace candlewick {
namespace runtime {

  /// \brief Message for intermediate representation of a vector or matrix.
  struct ArrayMessage {
    std::string dtype;
    std::vector<long> dims;
    std::vector<uint8_t> data;

    size_t ndim() const noexcept { return dims.size(); }

    MSGPACK_DEFINE(dtype, dims, data);
  };

  /// \brief Convert a ZMQ message (by move) to to a msgpack object.
  inline msgpack::object_handle get_handle_from_zmq_msg(zmq::message_t &&msg) {
    if (!msg.empty())
      return msgpack::unpack(static_cast<const char *>(msg.data()), msg.size());
    else
      return msgpack::object_handle();
  }

  namespace detail {
    template <typename S, typename T>
    using add_const_if_const_t =
        std::conditional_t<std::is_const_v<S>, std::add_const_t<T>, T>;

    template <typename MatrixType>
    Eigen::Map<MatrixType> get_eigen_view_msg_impl(
        const ArrayMessage &spec,
        add_const_if_const_t<MatrixType, typename MatrixType::Scalar> *data) {
      const Eigen::Index rows = spec.dims[0];
      using MapType = Eigen::Map<MatrixType>;
      if constexpr (MatrixType::IsVectorAtCompileTime) {
        return MapType{data, rows};
      } else {
        const Eigen::Index cols =
            spec.ndim() == 2 ? spec.dims[1]
                             : 1; // assume runtime vector if ndim wrong
        return MapType{data, rows, cols};
      }
    }
  } // namespace detail

  /// \brief Convert ArrayMessage to a mutable Eigen::Matrix view.
  template <typename MatrixType>
  Eigen::Map<MatrixType> get_eigen_view_from_spec(ArrayMessage &spec) {
    using Scalar = typename MatrixType::Scalar;
    Scalar *data = reinterpret_cast<Scalar *>(spec.data.data());
    return detail::get_eigen_view_msg_impl<MatrixType>(spec, data);
  }

  /// \copydoc get_eigen_view_from_spec()
  template <typename MatrixType>
  Eigen::Map<const MatrixType>
  get_eigen_view_from_spec(const ArrayMessage &spec) {
    using Scalar = typename MatrixType::Scalar;
    const Scalar *data = reinterpret_cast<const Scalar *>(spec.data.data());
    return detail::get_eigen_view_msg_impl<const MatrixType>(spec, data);
  }

} // namespace runtime
} // namespace candlewick
