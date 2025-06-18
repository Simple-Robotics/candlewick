#pragma once

#include "Core.h"
#include "math_types.h"

#include <Eigen/Geometry>

namespace candlewick {

namespace math {
  inline Mat3f computeNormalMatrix(const Eigen::Affine3f &M) {
    return M.inverse().linear().transpose();
  }
} // namespace math

enum class CameraProjection { PERSPECTIVE, ORTHOGRAPHIC };

/// \brief The main way of using a camera to render things.
struct Camera {
  /// Projection matrix \f$\mathbf{P}\f$
  Mat4f projection;
  /// %Camera view matrix \f$\mathbf{V}\f$
  Eigen::Isometry3f view;

  /// Compute and return the camera pose matrix \f$\mathbf{M} =
  /// \mathbf{V}^{-1}\f$
  Eigen::Isometry3f pose() const { return view.inverse(); }
  Float3 position() const { return pose().translation(); }

  /// View-projection matrix \f$\mathbf{P} \cdot \mathbf{V}\f$
  auto viewProj() const { return projection * view.matrix(); }

  /// \brief Transform a vector to view-space.
  /// This applies the view matrix linear part.
  Float3 transformVector(const Float3 &v) const { return view.linear() * v; }

  /// \brief Transform a point to view-space.
  /// This applies the view matrix as a 3D transform.
  Float3 transformPoint(const Float3 &p) const { return view * p; }

  /// \name Shortcuts for the basis vectors of the pose matrix
  /// i.e. the world-space camera orientation vectors.
  /// \{

  Float3 right() const { return view.linear().row(0); }
  auto right() { return view.linear().row(0); }
  Float3 up() const { return view.linear().row(1); }
  auto up() { return view.linear().row(1); }
  Float3 forward() const { return -view.linear().row(2); }

  /// \}
};

/// \name Camera view-projection utilities.
/// These functions should be used to construct Camera objects.
/// \{

/// Compute view matrix looking at \p center from \p eye, with
/// the camera pointing up towards \p up.
Mat4f lookAt(const Float3 &eye, const Float3 &center,
             const Float3 &up = Float3::UnitZ());

/// \brief Compute perspective projection matrix, from clipping plane
/// parameters (left, right, bottom, top, near, far).
Mat4f perspectiveMatrix(float left, float right, float bottom, float top,
                        float near, float far);

/// \brief Get perspective projection matrix given fov, aspect ratio, and
/// clipping planes.
/// \param fovY Vertical field of view in radians
/// \param aspectRatio Width / Height
/// \param nearZ Near clipping plane
/// \param farZ Far clipping plane
/// \warning This function uses the *vertical* field of view.
Mat4f perspectiveFromFov(Radf fovY, float aspectRatio, float nearZ, float farZ);

/// \brief Compute a **centered** orthographic projection matrix.
///
/// \param size xy-plane view sizes
/// \param nearZ Near clipping plane. This is where the rendering starts on the
/// Z-axis (the positive direction of which points up towards you).
/// \param farZ Far clipping plane, where rendering ends. A value of \f$0\f$
/// stops the rendered volume at the camera (_only_ things in front of camera
/// will be rendered).
Mat4f orthographicMatrix(const Float2 &sizes, float nearZ, float farZ);

/// \brief Compute an off-center orthographic projection matrix.
Mat4f orthographicMatrix(float left, float right, float bottom, float top,
                         float near, float far);

inline float orthoProjNear(const Mat4f &proj) {
  return (proj(2, 2) + 1.0f) / proj(2, 2);
}

inline float orthoProjFar(const Mat4f &proj) {
  return (proj(2, 3) - 1.0f) / proj(2, 2);
}

inline float perspectiveProjNear(const Mat4f &proj) {
  return proj(2, 3) / (proj(2, 2) - 1.f);
}

inline float perspectiveProjFar(const Mat4f &proj) {
  return std::abs(proj(2, 3) / (proj(2, 2) + 1.f));
}

/// \brief Extract the array of frustum corners, given the camera
/// view-projection matrix.
inline FrustumCornersType frustumFromCameraViewProj(const Mat4f &camProj) {
  auto invProj = camProj.inverse();
  FrustumCornersType out;
  for (Uint8 i = 0; i < 8; i++) {
    Uint8 j = i & 1;
    Uint8 k = (i >> 1) & 1;
    Uint8 l = (i >> 2) & 1;
    assert(j <= 1);
    assert(k <= 1);
    assert(l <= 1);

    Float4 ndc{2.f * j - 1.f, 2.f * k - 1.f, 2.f * l - 1.f, 1.f};
    Float4 viewSpace = invProj * ndc;
    out[i] = viewSpace.head<3>() / viewSpace.w();
  }
  return out;
}

inline std::pair<Float3, float>
frustumBoundingSphereCenterRadius(const FrustumCornersType &worldSpaceCorners) {
  Float3 frustumCenter = Float3::Zero();
  for (auto &c : worldSpaceCorners) {
    frustumCenter += c;
  }
  frustumCenter /= 8.f;

  float radius = 0.f;
  for (auto &c : worldSpaceCorners) {
    radius = std::max(radius, (c - frustumCenter).norm());
  }
  return {frustumCenter, radius};
}

inline void frustumApplyTransform(FrustumCornersType &corners,
                                  const Mat4f &tr) {
  for (auto &x : corners) {
    x = (tr * x.homogeneous()).head<3>();
  }
}

/// \}

} // namespace candlewick
