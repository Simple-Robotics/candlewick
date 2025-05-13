#include "candlewick/core/Core.h"
#include "candlewick/core/Device.h"
#include "candlewick/core/math_types.h"
#include "candlewick/utils/Utils.h"

#include <SDL3/SDL_gpu.h>

using candlewick::Device;
using candlewick::Float2;
using candlewick::MeshData;
using candlewick::NoInit;

const float kScrollZoom = 0.05f;

MeshData loadCube(float size, const Float2 &loc);
