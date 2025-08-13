#pragma once

#include "Scene.h"
#include "Mesh.h"
#include "RenderContext.h"
#include "math_types.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>

namespace candlewick {

enum class DebugPipelines {
  TRIANGLE_FILL,
  TRIANGLE_LINE,
};

class DebugScene;

enum class DebugMeshType {
  TRIAD,
  GRID,
  ARROW,
};

/// \brief A subsystem for the DebugScene.
///
/// Provides methods for updating debug entities.
/// \sa DebugScene
struct IDebugSubSystem {
  virtual void update() = 0;
  virtual ~IDebugSubSystem() = default;

protected:
  DebugScene &m_scene;
  explicit IDebugSubSystem(DebugScene &scene) : m_scene(scene) {}
};

/// \brief Component for simple mesh with colors.
///
/// This is meant for the \c Hud3dElement shader.
struct DebugMeshComponent;

/// \brief %Scene for organizing debug entities and render systems.
///
/// This implements a basic render system for DebugMeshComponent.
///
/// This scene and all subsystems are assumed to use the same shaders and
/// pipelines.
class DebugScene {
  entt::registry &m_registry;
  const RenderContext &m_renderer;
  SDL_GPUGraphicsPipeline *m_trianglePipeline{nullptr};
  SDL_GPUGraphicsPipeline *m_linePipeline{nullptr};
  entt::dense_map<entt::hashed_string::hash_type,
                  std::unique_ptr<IDebugSubSystem>>
      m_subsystems;
  std::unordered_map<DebugMeshType, Mesh> m_sharedMeshes;
  inline static const std::array<Float4, 3> m_triadColors = {
      Float4{1., 0., 0., 1.},
      Float4{0., 1., 0., 1.},
      Float4{0., 0., 1., 1.},
  };

  void initializeSharedMeshes();

  void setupPipelines(const MeshLayout &layout);

public:
  enum : Uint32 { TRANSFORM_SLOT = 0 };
  enum : Uint32 { COLOR_SLOT = 0 };
  using enum DebugMeshType;

  const Mesh &getMesh(DebugMeshType type) const {
    return m_sharedMeshes.at(type);
  }

  DebugScene(entt::registry &registry, const RenderContext &renderer);
  DebugScene(const DebugScene &) = delete;
  DebugScene &operator=(const DebugScene &) = delete;
  DebugScene(DebugScene &&other);
  DebugScene &operator=(DebugScene &&) = delete;

  const Device &device() const noexcept { return m_renderer.device; }
  entt::registry &registry() { return m_registry; }
  const entt::registry &registry() const { return m_registry; }

  /// \brief Add a subsystem (IDebugSubSystem) to the scene.
  template <std::derived_from<IDebugSubSystem> T, typename... Args>
  T &addSystem(entt::hashed_string::hash_type name, Args &&...args) {
    auto sys = std::make_unique<T>(*this, std::forward<Args>(args)...);
    auto [it, flag] = m_subsystems.emplace(name, std::move(sys));
    return static_cast<T &>(*it->second);
  }

  /// \brief Get a subsystem by key (a hashed string)
  /// \tparam T The concrete derived type.
  template <std::derived_from<IDebugSubSystem> T>
  T *getSystem(entt::hashed_string::hash_type name) {
    if (auto it = m_subsystems.find(name); it != m_subsystems.cend()) {
      return dynamic_cast<T *>(it->second.get());
    }
    return nullptr;
  }

  /// \sa getSystem().
  template <std::derived_from<IDebugSubSystem> T>
  T &tryGetSystem(entt::hashed_string::hash_type name) {
    auto *p = m_subsystems.at(name).get();
    return dynamic_cast<T &>(*p);
  }

  /// \brief Just the basic 3D triad.
  std::tuple<entt::entity, DebugMeshComponent &>
  addTriad(const Float3 &scale = Float3::Ones());

  /// \brief Add a basic line grid.
  /// \param color Grid color.
  std::tuple<entt::entity, DebugMeshComponent &>
  addLineGrid(const Float4 &color = 0xE0A236FF_rgbaf);

  /// \brief Add an arrow debug entity.
  std::tuple<entt::entity, DebugMeshComponent &>
  addArrow(const Float4 &color = 0xEA2502FF_rgbaf);

  void update() {
    for (auto [hash, system] : m_subsystems) {
      system->update();
    }
  }

  void render(CommandBuffer &cmdBuf, const Camera &camera) const;

  void release();

  ~DebugScene() { release(); }
};
static_assert(Scene<DebugScene>);

struct DebugMeshComponent {
  DebugPipelines pipeline_type;
  DebugMeshType meshType;
  std::vector<Float4> colors;
  bool enable = true;
  Float3 scale = Float3::Ones();
};

namespace gui {
  void addDebugMesh(DebugMeshComponent &dmc,
                    bool enable_pipeline_switch = true);
}

} // namespace candlewick
