#include "Components.h"
#include <algorithm>

namespace candlewick {
bool updateTransparencyClassification(entt::registry &reg, entt::entity entity,
                                      const MeshMaterialComponent &mmc) {
  bool hasTransparency = std::ranges::any_of(mmc.materials, [](auto &material) {
    return material.baseColor.w() < 1.0f;
  });

  if (hasTransparency) {
    reg.remove<Opaque>(entity);
  } else {
    reg.emplace_or_replace<Opaque>(entity);
  }
  return hasTransparency;
}
} // namespace candlewick
