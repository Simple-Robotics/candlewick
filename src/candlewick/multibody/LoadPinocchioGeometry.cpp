#include "LoadPinocchioGeometry.h"
#include "LoadCoalPrimitives.h"
#include "../core/errors.h"
#include "../utils/LoadMesh.h"
#include "../utils/MeshTransforms.h"

#include <pinocchio/multibody/geometry.hpp>

namespace candlewick::multibody {

void loadGeometryObject(const pin::GeometryObject &gobj,
                        std::vector<MeshData> &meshData) {
  using namespace coal;

  const coal::CollisionGeometry &collgom = *gobj.geometry.get();
  const OBJECT_TYPE objType = collgom.getObjectType();

  Float4 meshColor = gobj.meshColor.cast<float>();
  Float3 meshScale = gobj.meshScale.cast<float>();

  Eigen::Affine3f T;
  T.setIdentity();
  T.scale(meshScale);
  switch (objType) {
  case OT_BVH: {
    loadSceneMeshes(gobj.meshPath.c_str(), meshData);
    break;
  }
  case OT_GEOM: {
    MeshData &md = meshData.emplace_back(loadCoalPrimitive(collgom));
    md.material.baseColor = meshColor;
    break;
  }
  case OT_HFIELD: {
    MeshData &md = meshData.emplace_back(loadCoalHeightField(collgom));
    md.material.baseColor = meshColor;
    break;
  }
  default:
    terminate_with_message("Unsupported object type.");
    break;
  }
  for (auto &data : meshData) {
    apply3DTransformInPlace(data, T);
  }
}

} // namespace candlewick::multibody
