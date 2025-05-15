/// \file Visualizer.h
/// \author ManifoldFR
/// \copyright 2025 INRIA
#pragma once

#include "Multibody.h"
#include "../core/Device.h"
#include "../core/Scene.h"
#include "../core/Renderer.h"
#include "../core/LightUniforms.h"
#include "../core/Collision.h"
#include "../core/DepthAndShadowPass.h"
#include "../core/Texture.h"
#include "../posteffects/SSAO.h"
#include "../utils/MeshData.h"
#include <magic_enum/magic_enum.hpp>

#include <entt/entity/fwd.hpp>
#include <coal/fwd.hh>
#include <pinocchio/multibody/fwd.hpp>

namespace candlewick {

/// \brief Terminate the application after encountering an invalid enum value.
template <typename T>
  requires std::is_enum_v<T>
[[noreturn]] void
invalid_enum(const char *msg, T type,
             std::source_location location = std::source_location::current()) {
  char out[64];
  SDL_snprintf(out, 64ul, "Invalid enum: %s - %s", msg,
               magic_enum::enum_name(type).data());
  terminate_with_message(out, location);
}

namespace multibody {

  /// \brief A system for updating the transform components for robot geometry
  /// entities.
  ///
  /// This will also update the mesh materials.
  ///
  /// Reads PinGeomObjComponent, updates TransformComponent.
  void updateRobotTransforms(entt::registry &registry,
                             const pin::GeometryModel &geom_model,
                             const pin::GeometryData &geom_data);

  /// \brief A render system for Pinocchio robot geometries using Pinocchio.
  ///
  /// This internally stores references to pinocchio::GeometryModel and
  /// pinocchio::GeometryData objects.
  class RobotScene final {
    [[nodiscard]] bool hasInternalPointers() const {
      return m_geomModel && m_geomData;
    }

    void renderPBRTriangleGeometry(CommandBuffer &command_buffer,
                                   const Camera &camera);

    void renderOtherGeometry(CommandBuffer &command_buffer,
                             const Camera &camera);

  public:
    enum PipelineType {
      PIPELINE_TRIANGLEMESH,
      PIPELINE_HEIGHTFIELD,
      PIPELINE_POINTCLOUD,
    };
    static constexpr size_t kNumPipelineTypes =
        magic_enum::enum_count<PipelineType>();
    enum VertexUniformSlots : Uint32 { TRANSFORM = 0 };
    enum FragmentUniformSlots : Uint32 { MATERIAL = 0, LIGHTING = 1 };

    /// Map hpp-fcl/coal collision geometry to desired pipeline type.
    static PipelineType pinGeomToPipeline(const coal::CollisionGeometry &geom);

    /// Map pipeline type to geometry primitive.
    static constexpr SDL_GPUPrimitiveType
    getPrimitiveTopologyForType(PipelineType type) {
      switch (type) {
      case PIPELINE_TRIANGLEMESH:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
      case PIPELINE_HEIGHTFIELD:
        return SDL_GPU_PRIMITIVETYPE_LINELIST;
      case PIPELINE_POINTCLOUD:
        return SDL_GPU_PRIMITIVETYPE_POINTLIST;
      }
    }

    template <PipelineType t> using pipeline_tag = entt::tag<t>;

    struct PipelineConfig {
      // shader set
      const char *vertex_shader_path;
      const char *fragment_shader_path;
      SDL_GPUCullMode cull_mode = SDL_GPU_CULLMODE_BACK;
      SDL_GPUFillMode fill_mode = SDL_GPU_FILLMODE_FILL;
    };
    struct Config {
      std::unordered_map<PipelineType, PipelineConfig> pipeline_configs = {
          {PIPELINE_TRIANGLEMESH,
           {
               .vertex_shader_path = "PbrBasic.vert",
               .fragment_shader_path = "PbrBasic.frag",
           }},
          {PIPELINE_HEIGHTFIELD,
           {
               .vertex_shader_path = "Hud3dElement.vert",
               .fragment_shader_path = "Hud3dElement.frag",
           }},
          // {PIPELINE_POINTCLOUD, {}}
      };
      bool enable_msaa = false;
      bool enable_shadows = true;
      bool enable_ssao = true;
      bool triangle_has_prepass = false;
      bool enable_normal_target = false;
      SDL_GPUSampleCount msaa_samples = SDL_GPU_SAMPLECOUNT_1;
      ShadowPassConfig shadow_config;
    };

    struct Pipelines {
      struct {
        SDL_GPUGraphicsPipeline *opaque = nullptr;
        SDL_GPUGraphicsPipeline *transparent = nullptr;
      } triangleMesh;
      SDL_GPUGraphicsPipeline *heightfield = nullptr;
      SDL_GPUGraphicsPipeline *pointcloud = nullptr;
      SDL_GPUGraphicsPipeline *wboitComposite = nullptr;
    } pipelines;

    DirectionalLight directionalLight;
    ssao::SsaoPass ssaoPass{NoInit};
    struct GBuffer {
      Texture normalMap{NoInit};
    } gBuffer;
    ShadowPassInfo shadowPass;
    AABB worldSpaceBounds;

    /// \brief Non-initializing constructor.
    RobotScene(entt::registry &registry, const Renderer &renderer);

    /// \brief Constructor which initializes the system.
    ///
    /// loadModels() will be called.
    RobotScene(entt::registry &registry, const Renderer &renderer,
               const pin::GeometryModel &geom_model,
               const pin::GeometryData &geom_data, Config config);

    RobotScene(const RobotScene &) = delete;

    void setConfig(const Config &config) {
      CDW_ASSERT(
          !m_initialized,
          "Cannot call setConfig() after render system was initialized.");
      m_config = config;
    }

    /// \brief Set the internal geometry model and data pointers, and load the
    /// corresponding models.
    void loadModels(const pin::GeometryModel &geom_model,
                    const pin::GeometryData &geom_data);

    /// \brief Update the transform component of the GeometryObject entities.
    void updateTransforms();

    void collectOpaqueCastables();
    const std::vector<OpaqueCastable> &castables() const { return m_castables; }

    entt::entity
    addEnvironmentObject(MeshData &&data, Mat4f placement,
                         PipelineType pipe_type = PIPELINE_TRIANGLEMESH);

    entt::entity
    addEnvironmentObject(MeshData &&data, const Eigen::Affine3f &T,
                         PipelineType pipe_type = PIPELINE_TRIANGLEMESH) {
      return addEnvironmentObject(std::move(data), T.matrix(), pipe_type);
    }

    /// \brief Destroy all entities with the EnvironmentTag component.
    void clearEnvironment();
    /// \brief Destroy all entities with the PinGeomObjComponent component
    /// (Pinocchio geometry objects).
    void clearRobotGeometries();

    void createPipeline(const MeshLayout &layout,
                        SDL_GPUTextureFormat render_target_format,
                        SDL_GPUTextureFormat depth_stencil_format,
                        PipelineType type, bool transparent);

    /// \warning Call updateTransforms() before rendering the objects with
    /// this function.
    void render(CommandBuffer &command_buffer, const Camera &camera);

    /// \brief Release all resources.
    void release();

    Config &config() { return m_config; }
    const Config &config() const { return m_config; }
    inline bool pbrHasPrepass() const { return m_config.triangle_has_prepass; }
    inline bool shadowsEnabled() const { return m_config.enable_shadows; }

    /// \brief Getter for the pinocchio GeometryModel object.
    const pin::GeometryModel &geomModel() const { return *m_geomModel; }

    /// \brief Getter for the pinocchio GeometryData object.
    const pin::GeometryData &geomData() const { return *m_geomData; }

    const entt::registry &registry() const { return m_registry; }

    const Device &device() { return m_renderer.device; }

    SDL_GPUGraphicsPipeline *getPipeline(PipelineType type,
                                         bool transparent = false) {
      return *routePipeline(type, transparent);
    }

  private:
    entt::registry &m_registry;
    const Renderer &m_renderer;
    Config m_config;
    const pin::GeometryModel *m_geomModel;
    const pin::GeometryData *m_geomData;
    std::vector<OpaqueCastable> m_castables;
    bool m_initialized;

    SDL_GPUGraphicsPipeline **routePipeline(PipelineType type,
                                            bool transparent) {
      switch (type) {
      case PIPELINE_TRIANGLEMESH:
        return transparent ? &pipelines.triangleMesh.transparent
                           : &pipelines.triangleMesh.opaque;
      case PIPELINE_HEIGHTFIELD:
        return &pipelines.heightfield;
      case PIPELINE_POINTCLOUD:
        return &pipelines.pointcloud;
      }
    }
  };
  static_assert(Scene<RobotScene>);

} // namespace multibody
} // namespace candlewick
