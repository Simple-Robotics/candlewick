#pragma once

#include "../utils/Utils.h"
#include "../core/math_types.h"

#include <coal/fwd.hh>

namespace candlewick {

/// \brief Load primitive given a coal::CollisionGeometry.
///
/// See the documentation on the available primitives.
/// \sa primitives1
MeshData loadCoalPrimitive(const coal::CollisionGeometry &geometry);

MeshData loadCoalHeightField(const coal::CollisionGeometry &collGeom);

} // namespace candlewick
