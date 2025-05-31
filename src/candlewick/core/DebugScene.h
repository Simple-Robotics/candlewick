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
  entt::registry &_registry;
  const Renderer &_renderer;
  SDL_GPUGraphicsPipeline *_trianglePipeline;
  SDL_GPUGraphicsPipeline *_linePipeline;
  SDL_GPUTextureFormat _swapchainTextureFormat, _depthFormat;
  std::vector<std::unique_ptr<IDebugSubSystem>> _systems;

  void renderMeshComponents(CommandBuffer &cmdBuf,
                            SDL_GPURenderPass *render_pass,
                            const Camera &camera) const;

public:
  enum { TRANSFORM_SLOT = 0 };
  enum { COLOR_SLOT = 0 };

  DebugScene(entt::registry &registry, const Renderer &renderer);
  DebugScene(const DebugScene &) = delete;
  DebugScene &operator=(const DebugScene &) = delete;

  const Device &device() const noexcept { return _renderer.device; }
  entt::registry &registry() { return _registry; }
  const entt::registry &registry() const { return _registry; }

  /// \brief Add a subsystem (IDebugSubSystem) to the scene.
  template <std::derived_from<IDebugSubSystem> System, typename... Args>
  System &addSystem(Args &&...args) {
    auto sys = std::make_unique<System>(std::forward<Args>(args)...);
    _systems.push_back(std::move(sys));
    return static_cast<System &>(*_systems.back());
  }

  /// \brief Setup pipelines; this will only have an effect **ONCE**.
  void setupPipelines(const MeshLayout &layout);

  /// \brief Just the basic 3D triad.
  std::tuple<entt::entity, DebugMeshComponent &> addTriad();
  /// \brief Add a basic line grid.
  std::tuple<entt::entity, DebugMeshComponent &>
  addLineGrid(std::optional<Float4> color = std::nullopt);

  void update() {
    for (auto &system : _systems) {
      system->update(*this);
    }
  }

  void render(CommandBuffer &cmdBuf, const Camera &camera) const;

  void release();

  ~DebugScene() { release(); }
};
static_assert(Scene<DebugScene>);

} // namespace candlewick
