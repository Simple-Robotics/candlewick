#include "LoadCoalGeometries.h"

#include "../core/errors.h"
#include "../core/DefaultVertex.h"

#include "../primitives/Primitives.h"
#include "../utils/MeshTransforms.h"

#include <coal/shape/convex.h>
#include <coal/shape/geometric_shapes.h>
#include <coal/hfield.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_log.h>

namespace candlewick {

constexpr float kPlaneScale = 10.f;

static void
getPlaneOrHalfspaceNormalOffset(const coal::CollisionGeometry &geometry,
                                Float3 &n, float &d) {
  using namespace coal;
  switch (geometry.getNodeType()) {
  case GEOM_PLANE: {
    auto &g = castCoalGeom<Plane>(geometry);
    n = g.n.cast<float>();
    d = float(g.d);
    return;
  }
  case GEOM_HALFSPACE: {
    auto &g = castCoalGeom<Halfspace>(geometry);
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

MeshData loadCoalConvex(const coal::ConvexBase &geom_) {
  std::vector<DefaultVertex> vertexData;
  std::vector<MeshData::IndexType> indexData;

  auto &geom = dynamic_cast<const coal::Convex<coal::Triangle> &>(geom_);
  Uint32 num_vertices = geom.num_points;
  Uint32 num_tris = geom.num_polygons;

  vertexData.resize(num_vertices);
  indexData.resize(3 * num_tris);

  const auto &points = *geom.points;
  for (Uint32 i = 0; i < num_vertices; i++) {
    vertexData[i].pos = points[i].cast<float>();
    vertexData[i].color.setOnes();
  }

  const auto &faces = *geom.polygons;
  for (Uint32 i = 0; i < num_tris; i++) {
    auto f0 = indexData[3 * i + 0] = static_cast<Uint32>(faces[i][0]);
    auto f1 = indexData[3 * i + 1] = static_cast<Uint32>(faces[i][1]);
    auto f2 = indexData[3 * i + 2] = static_cast<Uint32>(faces[i][2]);

    Float3 p0 = vertexData[f0].pos;
    Float3 p1 = vertexData[f1].pos;
    Float3 p2 = vertexData[f2].pos;
    Float3 fn = (p2 - p1).cross(p0 - p1);
    vertexData[f0].normal += fn;
    vertexData[f1].normal += fn;
    vertexData[f2].normal += fn;
  }

  for (Uint32 i = 0; i < num_vertices; i++) {
    vertexData[i].pos.normalized();
  }

  return MeshData{SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, std::move(vertexData),
                  std::move(indexData)};
}

MeshData loadCoalPrimitive(const coal::ShapeBase &geometry) {
  using namespace coal;
  CANDLEWICK_ASSERT(geometry.getObjectType() == OT_GEOM,
                    "CollisionGeometry object type must be OT_GEOM !");
  MeshData meshData{NoInit};
  Eigen::Affine3f transform = Eigen::Affine3f::Identity();
  const NODE_TYPE nodeType = geometry.getNodeType();
  switch (nodeType) {
  case GEOM_BOX: {
    auto &g = castCoalGeom<Box>(geometry);
    transform.scale(g.halfSide.cast<float>());
    meshData = loadCubeSolid().toOwned();
    break;
  }
  case GEOM_SPHERE: {
    auto &g = castCoalGeom<Sphere>(geometry);
    // sphere loader doesn't have a radius argument, so apply scaling
    transform.scale(float(g.radius));
    meshData = loadUvSphereSolid(12u, 24u);
    break;
  }
  case GEOM_TRIANGLE: {
    // auto &g = castGeom<TriangleP>(geometry);
    terminate_with_message("Geometry type \'GEOM_TRIANGLE\' not supported");
    break;
  }
  case GEOM_CONVEX: {
    auto &g = castCoalGeom<ConvexBase>(geometry);
    meshData = loadCoalConvex(g);
    break;
  }
  case GEOM_ELLIPSOID: {
    auto &g = castCoalGeom<Ellipsoid>(geometry);
    transform.scale(g.radii.cast<float>());
    meshData = loadUvSphereSolid(12u, 24u);
    break;
  }
  case GEOM_CAPSULE: {
    auto &g = castCoalGeom<Capsule>(geometry);
    const float length = 2 * static_cast<float>(g.halfLength);
    const float radius = static_cast<float>(g.radius);
    transform.scale(radius);
    meshData = loadCapsuleSolid(12u, 32u, length / radius);
    break;
  }
  case GEOM_CONE: {
    auto &g = castCoalGeom<Cone>(geometry);
    float length = 2 * float(g.halfLength);
    meshData = loadConeSolid(16u, float(g.radius), length);
    break;
  }
  case GEOM_CYLINDER: {
    auto &g = castCoalGeom<Cylinder>(geometry);
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
    transform.rotate(quat).translate(d * Float3::UnitZ()).scale(kPlaneScale);
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

MeshData loadCoalHeightField(const coal::HeightField<coal::AABB> &geom) {
  return detail::load_coal_heightfield_impl(geom);
}

MeshData loadCoalHeightField(const coal::HeightField<coal::OBBRSS> &geom) {
  return detail::load_coal_heightfield_impl(geom);
}
} // namespace candlewick
