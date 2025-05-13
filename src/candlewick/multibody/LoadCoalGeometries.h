#pragma once

#include "../utils/Utils.h"

#include <coal/fwd.hh>

namespace coal {
template <typename BV> class HeightField;
class OBBRSS;
} // namespace coal

namespace candlewick {

/// \brief Load primitive given a coal::CollisionGeometry.
/// The geometry must be of object type coal::OT_GEOM.
///
/// See the documentation on the available primitives.
/// \sa primitives1
MeshData loadCoalPrimitive(const coal::CollisionGeometry &geometry);

MeshData loadCoalHeightField(const coal::HeightField<coal::AABB> &collGeom);

MeshData loadCoalHeightField(const coal::HeightField<coal::OBBRSS> &collGeom);

} // namespace candlewick
