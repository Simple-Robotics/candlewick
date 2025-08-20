#include "LoadPinocchioGeometry.h"
#include "../core/LoadCoalGeometries.h"
#include "../core/errors.h"
#include "../utils/LoadMesh.h"

#include <pinocchio/multibody/geometry.hpp>
#include <coal/hfield.h>

namespace candlewick::multibody {

void loadGeometryObject(const pin::GeometryObject &gobj,
                        std::vector<MeshData> &meshData) {
  using namespace coal;

  const CollisionGeometry &collgom = *gobj.geometry.get();
  const OBJECT_TYPE objType = collgom.getObjectType();

  Float4 meshColor = gobj.meshColor.cast<float>();
  const char *meshPath = gobj.meshPath.c_str();
  bool overrideMaterial = gobj.overrideMaterial;

  switch (objType) {
  case OT_BVH: {
    loadSceneMeshes(meshPath, meshData);
    break;
  }
  case OT_GEOM: {
    const ShapeBase &shape = castCoalGeom<ShapeBase>(collgom);
    meshData.emplace_back(loadCoalPrimitive(shape));
    // always set material color for OT_GEOM
    overrideMaterial = true;
    break;
  }
  case OT_HFIELD: {
    MeshData md{NoInit};
    switch (collgom.getNodeType()) {
    case HF_AABB: {
      md = loadCoalHeightField(castCoalGeom<HeightField<AABB>>(collgom));
      break;
    }
    case HF_OBBRSS: {
      md = loadCoalHeightField(castCoalGeom<HeightField<OBBRSS>>(collgom));
      break;
    }
    default:
      terminate_with_message("Geometry must be a heightfield!");
    }
    meshData.push_back(std::move(md));
    break;
  }
  default:
    terminate_with_message("Unsupported object type.");
    break;
  }
  for (auto &data : meshData) {
    if (overrideMaterial)
      data.material.baseColor = meshColor;
  }
}

} // namespace candlewick::multibody
