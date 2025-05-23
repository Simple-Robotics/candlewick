#include "Plane.h"
#include "../core/DefaultVertex.h"
#include "../utils/MeshTransforms.h"

namespace candlewick {

// 3——1
// │ /│
// │/ │
// 2——0
const DefaultVertex vertexData[]{
    {{+1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, Float4::Zero(), {+1.f, 0.f, 0.f}},
    {{+1.f, +1.f, 0.f}, {0.f, 0.f, 1.f}, Float4::Zero(), {0.f, +1.f, 0.f}},
    {{-1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, Float4::Zero(), {-1.f, 0.f, 0.f}},
    {{-1.f, +1.f, 0.f}, {0.f, 0.f, 1.f}, Float4::Zero(), {0.f, +1.f, 0.f}},
};

constexpr Uint32 indexData[] = {0, 1, 2, //
                                2, 1, 3};

MeshDataView loadPlane() {
  return {SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, vertexData, indexData};
}

MeshData loadPlaneTiled(float scale, Uint32 xrepeat, Uint32 yrepeat,
                        bool centered) {
  MeshData dataOwned = loadPlane().toOwned();
  {
    // normalize to (-1,-1) -- (1,1)
    const Eigen::Translation3f tr{0.5, 0.5, 0.};
    apply3DTransformInPlace(dataOwned, Eigen::Affine3f(tr).scale(0.5));
  }
  std::vector<MeshData> meshes;
  meshes.reserve(xrepeat * yrepeat);
  for (Sint32 i = 0; i < Sint32(xrepeat); i++) {
    for (Sint32 j = 0; j < Sint32(yrepeat); j++) {
      MeshData &m = meshes.emplace_back(MeshData::copy(dataOwned));
      const Eigen::Translation3f tr{float(i) * scale, float(j) * scale, 0.f};
      apply3DTransformInPlace(m, Eigen::Affine3f(tr).scale(scale));
    }
  }
  MeshData out = mergeMeshes(meshes);
  if (centered) {
    float xc = 0.5f * scale * float(xrepeat);
    float yc = 0.5f * scale * float(yrepeat);
    const Eigen::Translation3f center{-xc, -yc, 0.f};
    apply3DTransformInPlace(out, Eigen::Affine3f{center});
  }
  return out;
}

} // namespace candlewick
