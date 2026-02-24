#pragma once

#include <SDL3/SDL_stdinc.h>
#include <coal/hfield.h>

/// \brief Generate a heightfield using Perlin noise.
/// \param seed Random seed
/// \param nx Number of subdivisions in x-direction
/// \param x_dim Full width of the heigthfield
std::shared_ptr<coal::HeightField<coal::AABB>>
generatePerlinNoiseHeightfield(Uint32 seed, Uint32 nx, float x_dim);
