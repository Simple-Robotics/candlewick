#pragma once

#include "Scene.h"
#include "Mesh.h"
#include "RenderContext.h"
#include "math_types.h"

#include <optional>
#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>

namespace candlewick {

enum class DebugPipelines {
  TRIANGLE_FILL,
  LINE,
};

class DebugScene;

/// \brief A subsystem for the DebugScene.
///
/// Provides methods for updating debug entities.
/// \sa DebugScene
struct IDebugSubSystem {
  virtual void update(DebugScene & /*scene*/) = 0;
  virtual ~IDebugSubSystem() = default;
};

struct DebugMeshComponent {
  DebugPipelines pipeline_type;
  Mesh mesh;
  // fragment shader
  std::vector<GpuVec4> colors;
  bool enable = true;
  Float3 scale = Float3::Ones();
};

/// \brief %Scene for organizing debug entities and render systems.
///
/// This implements a basic render system for DebugMeshComponent.
class DebugScene {
  entt::registry &m_registry;
  const RenderContext &m_renderer;
  SDL_GPUGraphicsPipeline *m_trianglePipeline;
  SDL_GPUGraphicsPipeline *m_linePipeline;
  std::vector<std::unique_ptr<IDebugSubSystem>> m_subsystems;

public:
  enum { TRANSFORM_SLOT = 0 };
  enum { COLOR_SLOT = 0 };

  DebugScene(entt::registry &registry, const RenderContext &renderer);
  DebugScene(const DebugScene &) = delete;
  DebugScene &operator=(const DebugScene &) = delete;

  const Device &device() const noexcept { return m_renderer.device; }
  entt::registry &registry() { return m_registry; }
  const entt::registry &registry() const { return m_registry; }

  /// \brief Add a subsystem (IDebugSubSystem) to the scene.
  template <std::derived_from<IDebugSubSystem> System, typename... Args>
  System &addSystem(Args &&...args) {
    auto sys = std::make_unique<System>(std::forward<Args>(args)...);
    auto &p = m_subsystems.emplace_back(std::move(sys));
    return static_cast<System &>(*p);
  }

  /// \brief Setup pipelines; this will only have an effect **ONCE**.
  void setupPipelines(const MeshLayout &layout);

  /// \brief Just the basic 3D triad.
  std::tuple<entt::entity, DebugMeshComponent &>
  addTriad(const Float3 &scale = Float3::Ones());
  /// \brief Add a basic line grid.
  std::tuple<entt::entity, DebugMeshComponent &>
  addLineGrid(std::optional<Float4> color = std::nullopt);

  void update() {
    for (auto &system : m_subsystems) {
      system->update(*this);
    }
  }

  void render(CommandBuffer &cmdBuf, const Camera &camera) const;

  void release();

  ~DebugScene() { release(); }
};
static_assert(Scene<DebugScene>);

} // namespace candlewick
