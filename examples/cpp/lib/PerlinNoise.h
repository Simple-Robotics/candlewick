#pragma once
#include "candlewick/core/math_types.h"
#include <array>
#include <SDL3/SDL_stdinc.h>

using candlewick::Float2;

/// \brief A 2D perlin noise generator
struct PerlinNoise {
  explicit PerlinNoise(Uint32 seed = 1);

  // 2D Perlin noise
  float noise(float x, float y) const;

  // permutation array
  std::array<Uint16, 512ul> perm;
  // gradients at grid points
  std::array<Float2, 256ul> gradients;
  std::array<float, 256ul> heights;

  Uint16 hash(Uint32 i, Uint32 j) const {
    return perm[perm[i & 255] + (j & 255)] & 255;
  }

  /// Lookup gradient in cell ( \p i, \p j ).
  Float2 get_gradient(Uint32 i, Uint32 j) const {
    return gradients[hash(i, j)];
  }
  float get_height(Uint32 i, Uint32 j) const { return heights[hash(i, j)]; }

  /// Perlin fade function, t^5 - 15t^4 + 10t^3
  static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
  static float fade(float x, float y) { return fade(x) * fade(y); }
};
