#pragma once

#include "math_types.h"

namespace candlewick {

struct DirectionalLight {
  Float3 direction;
  Float3 color;
  float intensity;
};
static_assert(std::is_standard_layout_v<DirectionalLight>);

} // namespace candlewick
