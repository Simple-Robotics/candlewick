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

/// \name Camera control functions.
/// These functions are prefixed `camera` but take the view matrix and
/// not the camera's pose matrix.
/// \{

inline void cameraLocalRotateX(Camera &camera, Rad<float> angle) {
  float c, s;
  c = std::cos(angle);
  s = std::sin(angle);
  Mat3f R;
  R << 1., 0, 0, //
      0, c, -s,  //
      0, s, +c;
  camera.view.linear().applyOnTheLeft(R);
}

/// \brief Rotate the camera around the center by a given increment.
inline void cameraWorldRotateZ(Camera &camera, Rad<float> angle) {
  float c, s;
  c = std::cos(angle);
  s = std::sin(angle);
  Mat3f R;
  R << c, -s, 0., //
      s, +c, 0.,  //
      0., 0., 1.;
  camera.view.linear().applyOnTheRight(R);
}

inline void cameraLocalTranslate(Camera &camera, const Float3 &tr) {
  camera.view.translation() += tr;
}

inline void cameraLocalTranslateX(Camera &camera, float step) {
  cameraLocalTranslate(camera, {step, 0, 0});
}

inline void cameraWorldTranslate(Camera &camera, const Float3 &tr) {
  auto R = camera.view.linear();
  auto p = camera.view.translation();
  p += R * tr;
}

inline void cameraWorldTranslateZ(Camera &camera, float step) {
  cameraWorldTranslate(camera, {0, 0, step});
}

inline void cameraSetWorldPosition(Camera &camera, const Float3 &pos) {
  auto R = camera.view.linear();
  camera.view.translation() = R * pos;
}

/// Compute view matrix looking at \p center from \p eye, with
/// the camera pointing up towards \p up.
Mat4f lookAt(const Float3 &eye, const Float3 &center,
             const Float3 &up = Float3::UnitZ());

/// \brief Compute orthographic projection matrix, from clipping plane
/// parameters (left, right, bottom, top, near, far).
Mat4f perspectiveMatrix(float left, float right, float bottom, float top,
                        float near, float far);

/// \brief Get perspective projection matrix given fov, aspect ratio, and
/// clipping planes.
/// \param fovY Vertical field of view in radians
/// \param aspectRatio Width / Height
/// \param nearZ Near clipping plane
/// \param farZ Far clipping plane
Mat4f perspectiveFromFov(Radf fovY, float aspectRatio, float nearZ, float farZ);

/// \brief Compute an orthographic projection matrix.
///
/// \param sx x-direction view size
/// \param sy y-direction view size
/// \param nearZ Near clipping plane. This is where the rendering starts on the
/// Z-axis (the positive direction of which points up towards you).
/// \param farZ Far clipping plane, where rendering ends. A value of \f$0\f$
/// stops the rendered volume at the camera (_only_ things in front of camera
/// will be rendered).
Mat4f orthographicMatrix(float sx, float sy, float nearZ, float farZ);

/// \copybrief orthographicMatrix()
inline Mat4f orthographicMatrix(float left, float right, float bottom,
                                float top, float near, float far) {
  float sx = right - left;
  float sy = top - bottom;
  return orthographicMatrix(sx, sy, near, far);
}

/// \brief Given world-space bounds and a view matrix, obtain an orthographic
/// projection matrix.
void orthoProjFromWorldBounds(const Mat4f &view, const AABB &worldSceneBounds,
                              Eigen::Ref<Mat4f> proj);

/// \brief Given world-space frustum corners and a view matrix, obtain an
/// orthographic projection matrix.
void orthoProjFromWorldFrustum(const Mat4f &view,
                               const FrustumCornersType &worldSpaceCorners,
                               Eigen::Ref<Mat4f> proj);

/// \}

struct CylinderCameraControl {
  Camera &camera;

  static constexpr bool DEFAULT_Y_INVERT = true;

  auto &dolly(float height) {
    cameraWorldTranslateZ(camera, height);
    return *this;
  }

  auto &orbit(Radf angle) {
    cameraWorldRotateZ(camera, angle);
    return *this;
  }

  auto &viewportDrag(Float2 step, float rotSensitivity, float panSensitivity,
                     bool yinvert = DEFAULT_Y_INVERT) {
    const float rotStep = step.x() * rotSensitivity;
    const float panStep = step.y() * panSensitivity;
    float ystep = yinvert ? -panStep : panStep;
    return dolly(ystep).orbit(Rad{rotStep});
  }

  auto &moveInOut(float scale, float offset) {
    const float alpha = 1.f - (offset > 0 ? 1.f / scale : scale);
    const float curDist = camera.view.translation().norm();
    camera.view.matrix()(2, 3) += curDist * alpha;
    return *this;
  }
};

inline FrustumCornersType frustumFromCameraProjection(const Mat4f &camProj) {
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

} // namespace candlewick
