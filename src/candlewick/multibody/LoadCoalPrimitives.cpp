#include "LoadCoalPrimitives.h"

#include "../core/errors.h"

#include "../primitives/Primitives.h"
#include "../utils/MeshTransforms.h"

#include <coal/shape/geometric_shapes.h>
#include <coal/hfield.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_log.h>

namespace candlewick {

constexpr float kPlaneScale = 10.f;

// helper
template <typename T>
decltype(auto) castGeom(const coal::CollisionGeometry &geometry) {
  return static_cast<const T &>(geometry);
}

static void
getPlaneOrHalfspaceNormalOffset(const coal::CollisionGeometry &geometry,
                                Float3 &n, float &d) {
  using namespace coal;
  switch (geometry.getNodeType()) {
  case GEOM_PLANE: {
    auto &g = castGeom<Plane>(geometry);
    n = g.n.cast<float>();
    d = float(g.d);
    return;
  }
  case GEOM_HALFSPACE: {
    auto &g = castGeom<Halfspace>(geometry);
    n = g.n.cast<float>();
    d = float(g.d);
    return;
  }
  default:
    unreachable_with_message(
        "This function should not be called with a "
        "non-Plane, non-Halfspace coal CollisionGeometry.");
  }
}

MeshData loadCoalPrimitive(const coal::CollisionGeometry &geometry) {
  using namespace coal;
  SDL_assert_always(geometry.getObjectType() == OT_GEOM);
  MeshData meshData{NoInit};
  Eigen::Affine3f transform = Eigen::Affine3f::Identity();
  const NODE_TYPE nodeType = geometry.getNodeType();
  switch (nodeType) {
  case GEOM_BOX: {
    auto &g = castGeom<Box>(geometry);
    transform.scale(g.halfSide.cast<float>());
    meshData = loadCubeSolid().toOwned();
    break;
  }
  case GEOM_SPHERE: {
    auto &g = castGeom<Sphere>(geometry);
    // sphere loader doesn't have a radius argument, so apply scaling
    transform.scale(float(g.radius));
    meshData = loadUvSphereSolid(8u, 16u);
    break;
  }
  case GEOM_ELLIPSOID: {
    auto &g = castGeom<Ellipsoid>(geometry);
    transform.scale(g.radii.cast<float>());
    meshData = loadUvSphereSolid(8u, 16u);
    break;
  }
  case GEOM_CAPSULE: {
    auto &g = castGeom<Capsule>(geometry);
    const float length = static_cast<float>(2 * g.halfLength);
    transform.scale(float(g.radius));
    meshData = loadCapsuleSolid(6u, 16u, length);
    break;
  }
  case GEOM_CONE: {
    auto &g = castGeom<Cone>(geometry);
    float length = 2 * float(g.halfLength);
    meshData = loadConeSolid(16u, float(g.radius), length);
    break;
  }
  case GEOM_CYLINDER: {
    auto &g = castGeom<Cylinder>(geometry);
    float height = 2.f * float(g.halfLength);
    meshData = loadCylinderSolid(6u, 16u, float(g.radius), height);
    break;
  }
  case GEOM_HALFSPACE:
    [[fallthrough]];
  case GEOM_PLANE: {
    meshData = loadPlane().toOwned();
    Float3 n;
    float d;
    getPlaneOrHalfspaceNormalOffset(geometry, n, d);
    const auto quat = Eigen::Quaternionf::FromTwoVectors(Float3::UnitZ(), n);
    transform.scale(kPlaneScale).rotate(quat).translate(d * Float3::UnitZ());
    break;
  }
  default:
    terminate_with_message("Unsupported geometry type.");
    break;
  }
  // apply appropriate transform for converting to Coal geometry's scaling
  // parameters
  apply3DTransformInPlace(meshData, transform);
  return meshData;
}

namespace detail {
  template <typename BV>
  MeshData load_coal_heightfield_impl(const coal::HeightField<BV> &hf) {
    Eigen::MatrixXf heights = hf.getHeights().template cast<float>();
    Eigen::VectorXf xgrid = hf.getXGrid().template cast<float>();
    Eigen::VectorXf ygrid = hf.getYGrid().template cast<float>();

    return loadHeightfield(heights, xgrid, ygrid);
  }
} // namespace detail

MeshData loadCoalHeightField(const coal::CollisionGeometry &collGeom) {
  SDL_assert(collGeom.getObjectType() == coal::OT_HFIELD);
  const coal::NODE_TYPE nodeType = collGeom.getNodeType();
  switch (nodeType) {
  case coal::HF_AABB:
  case coal::BV_AABB: {
    auto &geom = castGeom<coal::HeightField<coal::AABB>>(collGeom);
    return detail::load_coal_heightfield_impl(geom);
  }
  case coal::HF_OBBRSS:
  case coal::BV_OBBRSS: {
    auto &geom = castGeom<coal::HeightField<coal::OBBRSS>>(collGeom);
    return detail::load_coal_heightfield_impl(geom);
  }
  default:
    terminate_with_message("Unsupported nodeType.");
  }
}
} // namespace candlewick
