#pragma once

#include "Multibody.h"
#include "../utils/MeshData.h"

#include <pinocchio/multibody/geometry-object.hpp>

namespace candlewick::multibody {

/// \brief Load an invidual pinocchio::GeometryObject 's component geometries
/// into an array of \c MeshData.
///
/// \note
/// The mesh materials' base colors will be overriden by the
/// pinocchio::GeometryObject::meshColor attribute if either of the following
/// are true:
///   - the geometry's object type is coal::OT_GEOM,
///   - the pinocchio::GeometryObject::overrideMaterial flag is set to \c true.
///
void loadGeometryObject(const pin::GeometryObject &gobj,
                        std::vector<MeshData> &meshData);

/// \copydoc loadGeometryObject(const pin::GeometryObject&,
/// std::vector<MeshData> &)
inline std::vector<MeshData>
loadGeometryObject(const pin::GeometryObject &gobj) {
  std::vector<MeshData> meshData;
  loadGeometryObject(gobj, meshData);
  return meshData;
}

} // namespace candlewick::multibody
