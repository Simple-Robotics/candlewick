#include "GenHeightfield.h"
#include "PerlinNoise.h"
#include <coal/hfield.h>

std::shared_ptr<coal::HeightField<coal::AABB>>
generatePerlinNoiseHeightfield(Uint32 seed, Uint32 nx, float x_dim) {
  PerlinNoise noise{seed};

  const Uint32 ny = nx;
  const float y_dim = x_dim;
  Eigen::MatrixXf heights{nx, ny};
  heights.setZero();

  std::vector amplitudes = {0.1f, 0.3f};
  float base = 2.f;
  size_t num_octaves = amplitudes.size();

  float x, y;
  for (Uint32 i = 0; i < nx; i++) {
    x = float(i) * x_dim;
    for (Uint32 j = 0; j < ny; j++) {
      y = float(j) * y_dim;
      for (auto k = 0ul; k < num_octaves; k++) {
        float w = powf(base, float(k));
        heights(i, j) += amplitudes[k] * noise.noise(w * x, w * y);
      }
    }
  }
  assert(!heights.hasNaN());
  return std::make_shared<coal::HeightField<coal::AABB>>(
      x_dim, y_dim, heights.cast<double>());
}
