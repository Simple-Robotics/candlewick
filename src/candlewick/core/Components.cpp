#include "Components.h"
#include <algorithm>

namespace candlewick {
bool MeshMaterialComponent::hasTransparency() const {
  return std::ranges::any_of(
      materials, [](auto &material) { return material.baseColor.w() < 1.0f; });
}

bool updateTransparencyClassification(entt::registry &reg, entt::entity entity,
                                      const MeshMaterialComponent &mmc) {
  bool is_transparent = mmc.hasTransparency();
  if (is_transparent) {
    reg.remove<Opaque>(entity);
  } else {
    reg.emplace_or_replace<Opaque>(entity);
  }
  return is_transparent;
}
} // namespace candlewick
