#pragma once
#include <Eigen/Core>
#include <SDL3/SDL_stdinc.h>

namespace candlewick {

using Float2 = Eigen::Vector2f;
using Float3 = Eigen::Vector3f;
using Float4 = Eigen::Vector4f;
using Mat3f = Eigen::Matrix3f;
using Mat4f = Eigen::Matrix4f;
using Vec3u8 = Eigen::Matrix<Uint8, 3, 1>;
using Vec4u8 = Eigen::Matrix<Uint8, 4, 1>;

using FrustumCornersType = std::array<Float3, 8ul>;

using GpuVec2 = Eigen::Matrix<float, 2, 1, Eigen::DontAlign>;
using GpuVec3 = Eigen::Matrix<float, 3, 1, Eigen::DontAlign>;
using GpuVec4 = Eigen::Matrix<float, 4, 1, Eigen::DontAlign>;
// adapter type instead of typedef, to fit GLSL layout
struct GpuMat3 {
  /* implicit */ GpuMat3(Eigen::Matrix3f value) : _data() {
    _data.topRows<3>() = value;
  }
  operator auto &() { return _data; }
  operator const auto &() const { return _data; }

private:
  Eigen::Matrix<float, 4, 3, Eigen::DontAlign> _data;
};
using GpuMat4 = Eigen::Matrix<float, 4, 4, Eigen::ColMajor | Eigen::DontAlign>;

namespace constants {
  inline constexpr double Pi = 3.1415926535897932;
  inline constexpr float Pif = 3.141592654f;
  inline constexpr double Pi_2 = 1.5707963267948966;
  inline constexpr float Pi_2f = 1.5707963267f;
} // namespace constants

inline constexpr double deg2rad(double t) { return t * constants::Pi / 180.0; }
inline constexpr float deg2rad(float t) { return t * constants::Pif / 180.0f; }
inline constexpr float rad2deg(float t) { return t * 180.0f / constants::Pif; }

/// \defgroup angle-strong Angle strong types
/// \brief Strong types for angle quantities.
///
/// These are used to explicitly mark a floating point value as being degrees or
/// radians. This avoids passing values in degrees to functions expecting
/// radians (which use e.g. trigonometric functions internally).
/// \{

template <std::floating_point T> struct Rad;
template <std::floating_point T> struct Deg;

/// \brief Strong type for floating-point variables representing angles (in
/// **radians**).
/// \sa Deg
template <std::floating_point T> struct Rad {
  constexpr Rad() : _value(static_cast<T>(0.)) {}
  constexpr Rad(T value) : _value(value) {}
  constexpr Rad(Deg<T> value) : _value(deg2rad(T(value))) {}
  constexpr operator T &() { return _value; }
  constexpr operator T() const { return _value; }
  /* implicit */ constexpr operator T *() { return &_value; }
  template <typename U> constexpr bool operator==(const Rad<U> &other) const {
    return _value == other._value;
  }
  template <typename U> constexpr bool operator==(const Deg<U> &other) const {
    return (*this) == Rad(other);
  }

private:
  T _value;
};
template <std::floating_point T> Rad(T) -> Rad<T>;

/// \brief Strong type for floating-point variables representing angles (in
/// **degrees**).
/// \sa Rad
template <std::floating_point T> struct Deg {
  constexpr Deg() : _value(static_cast<T>(0.)) {}
  constexpr Deg(T value) : _value(value) {}
  constexpr Deg(Rad<T> value) : _value(rad2deg(T(value))) {}
  constexpr operator T &() { return _value; }
  constexpr operator T() const { return _value; }
  /* implicit */ constexpr operator T *() { return &_value; }
  template <typename U> constexpr bool operator==(const Deg<U> &other) const {
    return _value == other._value;
  }
  template <typename U> constexpr bool operator==(const Rad<U> &other) const {
    return _value == Deg(other);
  }

private:
  T _value;
};
template <std::floating_point T> Deg(T) -> Deg<T>;

template <std::floating_point T>
constexpr Rad<T> operator*(const Rad<T> &left, const T &right) {
  return Rad<T>{T(left) * right};
}

template <std::floating_point T>
constexpr Rad<T> operator*(const T &left, const Rad<T> &right) {
  return Rad<T>{left * T(right)};
}

inline constexpr auto operator""_radf(long double t) {
  return Rad<float>(static_cast<float>(t));
}

inline constexpr auto operator""_rad(long double t) {
  return Rad<double>(static_cast<double>(t));
}

inline constexpr auto operator""_deg(long double t) {
  return Deg<double>{static_cast<double>(t)};
}

inline constexpr auto operator""_degf(long double t) {
  return Deg<float>{static_cast<float>(t)};
}

using Radf = Rad<float>;
using Degf = Deg<float>;

extern template struct Rad<float>;
extern template struct Deg<float>;

/// \}

Vec3u8 hexToRgbi(unsigned long hex);

Vec4u8 hexToRgbai(unsigned long hex);

inline Float3 hexToRgbf(unsigned long hex) {
  return hexToRgbi(hex).cast<float>() / 255.f;
};

inline Float4 hexToRgbaf(unsigned long hex) {
  return hexToRgbai(hex).cast<float>() / 255.f;
};

inline Float3 operator""_rgbf(unsigned long long hex) { return hexToRgbf(hex); }

inline Float4 operator""_rgbaf(unsigned long long hex) {
  return hexToRgbaf(hex);
}

inline Eigen::Vector3d operator""_rgb(unsigned long long hex) {
  return hexToRgbi(hex).cast<double>() / 255.;
}

inline Eigen::Vector4d operator""_rgba(unsigned long long hex) {
  return hexToRgbai(hex).cast<double>() / 255.;
}

namespace math {
  constexpr Uint32 roundUpTo16(Uint32 value) {
    Uint32 q = value / 16;
    Uint32 r = value % 16;
    if (r == 0)
      return value;
    return (q + 1) * 16u;
  }

  inline Mat3f computeNormalMatrix(const Mat4f &M) {
    return M.topLeftCorner<3, 3>().inverse().transpose();
  }
} // namespace math

class Mesh;

/// \brief Intermediary argument type for shadow-casting or opaque objects. For
/// use in depth or light pre-passes.
using OpaqueCastable = std::tuple<const Mesh &, Mat4f>;

} // namespace candlewick
