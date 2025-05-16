#pragma once
#include "math_types.h"
#include "Mesh.h"
#include "MaterialUniform.h"

namespace candlewick {

/// Tag struct for denoting an entity as opaque, for render pass organization.
struct Opaque {};

/// Tag struct for disabled (invisible) entities.
struct Disable {};

// Tag environment entities
struct EnvironmentTag {};

struct TransformComponent : Mat4f {
  using Mat4f::Mat4f;
  using Mat4f::operator=;
};

struct MeshMaterialComponent {
  Mesh mesh;
  std::vector<PbrMaterial> materials;
  MeshMaterialComponent(Mesh &&mesh, std::vector<PbrMaterial> &&materials)
      : mesh(std::move(mesh)), materials(std::move(materials)) {
    assert(mesh.numViews() == materials.size());
  }
};

/// \brief Updates (adds or removes) the Opaque tag component for a given
/// entity.
/// \param reg The entity registry
/// \param entity The entity
/// \param mmc The mesh-material element to inspect for existence of any
/// transparent subobjects.
/// \returns whether the entity is transparent.
bool updateTransparencyClassification(entt::registry &reg, entt::entity entity,
                                      const MeshMaterialComponent &mmc);

} // namespace candlewick
