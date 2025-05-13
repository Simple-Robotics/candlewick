#pragma once

#include "../utils/Utils.h"

#include <coal/fwd.hh>

namespace coal {
template <typename BV> class HeightField;
class OBBRSS;
class ShapeBase;
class ConvexBase;
} // namespace coal

namespace candlewick {

template <typename T>
decltype(auto) castCoalGeom(const coal::CollisionGeometry &geometry) {
#ifndef DEBUG
  return static_cast<const T &>(geometry);
#else
  return dynamic_cast<const T &>(geometry);
#endif
}

/// \brief Load primitive given a coal::CollisionGeometry.
/// The geometry must be of object type coal::OT_GEOM.
///
/// See the documentation on the available primitives.
/// \sa primitives1
MeshData loadCoalPrimitive(const coal::CollisionGeometry &geometry);

MeshData loadCoalConvex(const coal::ConvexBase &geom);

MeshData loadCoalHeightField(const coal::HeightField<coal::AABB> &collGeom);

MeshData loadCoalHeightField(const coal::HeightField<coal::OBBRSS> &collGeom);

} // namespace candlewick
