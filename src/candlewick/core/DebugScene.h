#pragma once

#include "Scene.h"
#include "Mesh.h"
#include "Renderer.h"
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

struct IDebugSubSystem {
  virtual void update(DebugScene &scene) = 0;
  virtual ~IDebugSubSystem() = default;
};

struct DebugScaleComponent {
  Float3 value;
};

struct DebugMeshComponent {
  DebugPipelines pipeline_type;
  Mesh mesh;
  // fragment shader
  std::vector<GpuVec4> colors;
  Mat4f transform;
  bool enable = true;
};

/// \brief %Scene for organizing debug entities and render systems.
///
/// This implements a basic render system for DebugMeshComponent.
class DebugScene final {
  const Device &_device;
  SDL_GPUGraphicsPipeline *_trianglePipeline;
  SDL_GPUGraphicsPipeline *_linePipeline;
  SDL_GPUTextureFormat _swapchainTextureFormat, _depthFormat;
  std::vector<entt::scoped_connection> _connections;
  entt::registry _registry;

  void on_destroy_mesh_component(entt::registry &registry, entt::entity);

public:
  enum { TRANSFORM_SLOT = 0 };
  enum { COLOR_SLOT = 0 };

  DebugScene(const Renderer &renderer);
  DebugScene(const DebugScene &) = delete;
  DebugScene &operator=(const DebugScene &) = delete;

  const Device &device() const noexcept { return _device; }
  entt::registry &registry() { return _registry; }
  const entt::registry &registry() const { return _registry; }

  /// \brief Setup pipelines; this will only have an effect **ONCE**.
  void setupPipelines(const MeshLayout &layout);

  /// \brief Just the basic 3D triad.
  std::tuple<entt::entity, DebugMeshComponent &> addTriad();
  /// \brief Add a basic line grid.
  std::tuple<entt::entity, DebugMeshComponent &>
  addLineGrid(std::optional<Float4> color = std::nullopt);

  void render(Renderer &renderer, const Camera &camera) const;

  void release();

  ~DebugScene() { release(); }
};

static_assert(Scene<DebugScene>);

} // namespace candlewick
