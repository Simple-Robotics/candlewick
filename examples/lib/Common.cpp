#include "candlewick/core/MeshLayout.h"
#include "candlewick/core/Shader.h"
#include "candlewick/core/math_types.h"
#include "candlewick/primitives/Cube.h"
#include "candlewick/utils/MeshTransforms.h"

#include "Common.h"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>

using namespace candlewick;

MeshData loadCube(float size, const Float2 &loc) {
  MeshData m = loadCubeSolid().toOwned();
  m.material.metalness = 0.4f;
  m.material.baseColor = 0xaab023ff_rgbaf;
  Eigen::Affine3f tr;
  tr.setIdentity();
  tr.translate(Float3{loc[0], loc[1], 0.f});
  tr.scale(0.5f * size);
  tr.translate(Float3{0, 0, 1.2f});
  apply3DTransformInPlace(m, tr);
  return m;
}
