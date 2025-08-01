#include "RobotScene.h"
#include "LoadPinocchioGeometry.h"
#include "../core/Components.h"
#include "../core/Shader.h"
#include "../core/Components.h"
#include "../core/TransformUniforms.h"
#include "../core/Camera.h"

#include <entt/entity/registry.hpp>
#include <coal/BVH/BVH_model.h>
#include <pinocchio/multibody/data.hpp>
#include <pinocchio/multibody/geometry.hpp>

#include <magic_enum/magic_enum_utility.hpp>
#include <magic_enum/magic_enum_switch.hpp>

namespace candlewick::multibody {

struct alignas(16) LightArrayUbo {
  GpuVec4 viewSpaceDir[kNumLights];
  GpuVec4 color[kNumLights];
  GpuVec4 intensity[kNumLights];
  Uint32 numLights;
};
static_assert(std::is_standard_layout_v<LightArrayUbo>);

struct alignas(16) LightSpaceMatricesUbo {
  GpuMat4 mvps[kNumLights];
  Uint32 numLights;
};

struct alignas(16) ShadowAtlasInfoUbo {
  using Vec4u = Eigen::Matrix<Uint32, 4, 1, Eigen::DontAlign>;
  std::array<Vec4u, kNumLights> regions;
};

void updateRobotTransforms(entt::registry &registry,
                           const pin::GeometryModel &geom_model,
                           const pin::GeometryData &geom_data) {
  auto view = registry.view<const PinGeomObjComponent, TransformComponent,
                            MeshMaterialComponent>();
  for (auto [ent, geom_id, tr, mmc] : view.each()) {
    auto &gobj = geom_model.geometryObjects[geom_id];
    SE3f pose = geom_data.oMg[geom_id].cast<float>();
    Float3 scale = gobj.meshScale.cast<float>();
    Float4 color = gobj.meshColor.cast<float>();
    auto D = scale.homogeneous().asDiagonal();
    tr.noalias() = pose.toHomogeneousMatrix() * D;
    if (gobj.overrideMaterial) {
      for (auto &mat : mmc.materials)
        mat.baseColor = color;
      if (color.w() < 1.0f) {
        registry.remove<Opaque>(ent);
      } else {
        registry.emplace_or_replace<Opaque>(ent);
      }
    }
  }
}

auto RobotScene::pinGeomToPipeline(const coal::CollisionGeometry &geom)
    -> PipelineType {
  using enum coal::OBJECT_TYPE;
  const auto objType = geom.getObjectType();
  switch (objType) {
  case OT_GEOM:
    return PIPELINE_TRIANGLEMESH;
  case OT_HFIELD:
    return PIPELINE_HEIGHTFIELD;
  case OT_BVH: {
    using enum coal::BVHModelType;
    // cast to BVHModelBase, check if mesh or points
    const auto &bvh = dynamic_cast<const coal::BVHModelBase &>(geom);
    switch (bvh.getModelType()) {
    case BVH_MODEL_POINTCLOUD:
      return PIPELINE_POINTCLOUD;
    case BVH_MODEL_TRIANGLES:
      return PIPELINE_TRIANGLEMESH;
    case BVH_MODEL_UNKNOWN:
      invalid_enum("Unknown BVH model type.", BVH_MODEL_UNKNOWN);
    }
  }
  case OT_COUNT:
  case OT_OCTREE:
  case OT_UNKNOWN:
    invalid_enum("Unsupported object type", objType);
  }
}

static void addPipelineTagComponent(entt::registry &reg, entt::entity ent,
                                    RobotScene::PipelineType type) {
  magic_enum::enum_switch(
      [&reg, ent](auto pt) { reg.emplace<RobotScene::pipeline_tag<pt>>(ent); },
      type);
}

entt::entity RobotScene::addEnvironmentObject(MeshData &&data, Mat4f placement,
                                              PipelineType pipe_type) {
  Mesh mesh = createMesh(device(), data);
  uploadMeshToDevice(device(), mesh, data);
  entt::entity entity = m_registry.create();
  m_registry.emplace<TransformComponent>(entity, placement);
  if (pipe_type != PIPELINE_POINTCLOUD)
    m_registry.emplace<Opaque>(entity);
  // add tag type
  m_registry.emplace<EnvironmentTag>(entity);
  const auto &mmc = m_registry.emplace<MeshMaterialComponent>(
      entity, std::move(mesh), std::vector{std::move(data.material)});
  updateTransparencyClassification(m_registry, entity, mmc);
  addPipelineTagComponent(m_registry, entity, pipe_type);
  return entity;
}

void RobotScene::clearEnvironment() {
  auto view = m_registry.view<EnvironmentTag>();
  m_registry.destroy(view.begin(), view.end());
}

void RobotScene::clearRobotGeometries() {
  auto view = m_registry.view<PinGeomObjComponent>();
  m_registry.destroy(view.begin(), view.end());
}

RobotScene::RobotScene(entt::registry &registry, const RenderContext &renderer)
    : m_registry(registry)
    , m_renderer(renderer)
    , m_config()
    , m_geomModel(nullptr)
    , m_geomData(nullptr)
    , m_initialized(false) {
  assert(!hasInternalPointers());
  SDL_zero(directionalLight);
  directionalLight[1].direction = {-1.f, 1.f, -1.f};
  directionalLight[1].color.setOnes();
  directionalLight[1].intensity = 4.f;
}

RobotScene::RobotScene(entt::registry &registry, const RenderContext &renderer,
                       const pin::GeometryModel &geom_model,
                       const pin::GeometryData &geom_data, Config config)
    : RobotScene(registry, renderer) {

  this->setConfig(config);
  this->loadModels(geom_model, geom_data);
}

void RobotScene::initGBuffer() {
  const auto [width, height] = m_renderer.window.size();
  gBuffer.normalMap = Texture{m_renderer.device,
                              {
                                  .type = SDL_GPU_TEXTURETYPE_2D,
                                  .format = SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT,
                                  .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                           SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                  .width = Uint32(width),
                                  .height = Uint32(height),
                                  .layer_count_or_depth = 1,
                                  .num_levels = 1,
                                  .sample_count = SDL_GPU_SAMPLECOUNT_1,
                                  .props = 0,
                              },
                              "GBuffer normal"};

  gBuffer.accumTexture =
      Texture{device(),
              {
                  .type = SDL_GPU_TEXTURETYPE_2D,
                  .format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
                  .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                           SDL_GPU_TEXTUREUSAGE_SAMPLER,
                  .width = Uint32(width),
                  .height = Uint32(height),
                  .layer_count_or_depth = 1,
                  .num_levels = 1,
                  .sample_count = SDL_GPU_SAMPLECOUNT_1,
                  .props = 0,
              },
              "WBOIT Accumulation"};

  gBuffer.revealTexture =
      Texture{device(),
              {
                  .type = SDL_GPU_TEXTURETYPE_2D,
                  .format = SDL_GPU_TEXTUREFORMAT_R8_UNORM,
                  .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                           SDL_GPU_TEXTUREUSAGE_SAMPLER,
                  .width = Uint32(width),
                  .height = Uint32(height),
                  .layer_count_or_depth = 1,
                  .num_levels = 1,
                  .sample_count = SDL_GPU_SAMPLECOUNT_1,
                  .props = 0,
              },
              "WBOIT Revelage"};

  SDL_GPUSamplerCreateInfo sic{
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  };
  gBuffer.sampler = SDL_CreateGPUSampler(device(), &sic);
}

void RobotScene::initCompositePipeline(const MeshLayout &layout) {
  Shader vertexShader = Shader::fromMetadata(device(), "DrawQuad.vert");
  Shader fragmentShader = Shader::fromMetadata(device(), "WBOITComposite.frag");

  SDL_GPUColorTargetDescription color_target;
  SDL_zero(color_target);
  color_target.format = m_renderer.getSwapchainTextureFormat();
  color_target.blend_state = {
      .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
      .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
      .color_blend_op = SDL_GPU_BLENDOP_ADD,
      .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
      .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
      .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
      .enable_blend = true,
  };

  SDL_GPUGraphicsPipelineCreateInfo desc{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
      .vertex_input_state = layout,
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state{
          .fill_mode = SDL_GPU_FILLMODE_FILL,
          .cull_mode = SDL_GPU_CULLMODE_NONE,
      },
      .depth_stencil_state{
          .enable_depth_test = false,
          .enable_depth_write = false,
      },
      .target_info{
          .color_target_descriptions = &color_target,
          .num_color_targets = 1,
          .has_depth_stencil_target = false,
      },
  };

  m_pipelines.wboitComposite = SDL_CreateGPUGraphicsPipeline(device(), &desc);
  if (!m_pipelines.wboitComposite) {
    terminate_with_message("Failed to create WBOIT pipeline: {:s}",
                           SDL_GetError());
  }
}

void RobotScene::ensurePipelinesExist(
    const std::set<std::tuple<MeshLayout, PipelineType, bool>>
        &required_pipelines) {
  if (!gBuffer.initialized()) {
    this->initGBuffer();
  }

  const bool enable_shadows = m_config.enable_shadows;

  for (auto &[layout, pipeline_type, transparent] : required_pipelines) {
    // We only route wrt pipeline type and opacity status.
    // If for some reason the set contains two entries for e.g. opaque triangle
    // pipeline with different mesh layouts, then the first entry wins because
    // it gets to create the pipeline.
    if (!routePipeline(pipeline_type, transparent)) {
      createRenderPipeline(layout, m_renderer.getSwapchainTextureFormat(),
                           m_renderer.depthFormat(), pipeline_type,
                           transparent);
    }

    if (pipeline_type == PIPELINE_TRIANGLEMESH) {
      if (!ssaoPass.pipeline) {
        ssaoPass = ssao::SsaoPass(m_renderer, gBuffer.normalMap);
      }
      // configure shadow pass
      if (enable_shadows && !shadowPass.initialized()) {
        shadowPass = ShadowMapPass(device(), layout, m_renderer.depthFormat(),
                                   m_config.shadow_config);
      }
      if (!m_pipelines.wboitComposite)
        this->initCompositePipeline(layout);
    }
  }
}

void RobotScene::loadModels(const pin::GeometryModel &geom_model,
                            const pin::GeometryData &geom_data) {
  if (hasInternalPointers())
    this->clearRobotGeometries();

  m_geomModel = &geom_model;
  m_geomData = &geom_data;

  // Phase 1. Load robot geometries and collect parameters for creating the
  // required render pipelines.
  std::set<std::tuple<MeshLayout, PipelineType, bool>> required_pipelines;

  for (pin::GeomIndex geom_id = 0; geom_id < geom_model.ngeoms; geom_id++) {

    const auto &geom_obj = geom_model.geometryObjects[geom_id];
    auto meshDatas = loadGeometryObject(geom_obj);
    PipelineType pipeline_type = pinGeomToPipeline(*geom_obj.geometry);
    Mesh mesh = createMeshFromBatch(device(), meshDatas, true);
    assert(validateMesh(mesh));

    // add entity for this geometry
    entt::entity entity = m_registry.create();
    m_registry.emplace<PinGeomObjComponent>(entity, geom_id);
    m_registry.emplace<TransformComponent>(entity);
    const MeshMaterialComponent &mmc =
        m_registry.emplace<MeshMaterialComponent>(entity, std::move(mesh),
                                                  extractMaterials(meshDatas));
    if (pipeline_type != PIPELINE_POINTCLOUD)
      m_registry.emplace<Opaque>(entity);
    bool is_transparent =
        updateTransparencyClassification(m_registry, entity, mmc);
    addPipelineTagComponent(m_registry, entity, pipeline_type);

    auto &layout = mmc.mesh.layout();
    required_pipelines.insert({layout, pipeline_type, is_transparent});
  }

  // Phase 2. Init our render pipelines.
  this->ensurePipelinesExist(required_pipelines);
  m_initialized = true;
}

void RobotScene::updateTransforms() {
  updateRobotTransforms(registry(), geomModel(), geomData());
}

void RobotScene::collectOpaqueCastables() {
  auto all_view = m_registry.view<const Opaque, const TransformComponent,
                                  const MeshMaterialComponent,
                                  pipeline_tag<PIPELINE_TRIANGLEMESH>>(
      entt::exclude<Disable>);

  m_castables.clear();

  // collect castable objects
  all_view.each([this](auto &&tr, auto &&meshMaterial) {
    m_castables.emplace_back(meshMaterial.mesh, tr);
  });
}

void RobotScene::renderOpaque(CommandBuffer &command_buffer,
                              const Camera &camera) {
  if (m_config.enable_ssao) {
    ssaoPass.render(command_buffer, camera);
  }

  renderPBRTriangleGeometry(command_buffer, camera, false);

  renderOtherGeometry(command_buffer, camera);
}

void RobotScene::renderTransparent(CommandBuffer &command_buffer,
                                   const Camera &camera) {
  renderPBRTriangleGeometry(command_buffer, camera, true);

  compositeTransparencyPass(command_buffer);
}

void RobotScene::render(CommandBuffer &command_buffer, const Camera &camera) {
  renderOpaque(command_buffer, camera);
  renderTransparent(command_buffer, camera);
}

/// Function private to this translation unit.
/// Utility function to provide a render pass handle
/// with just two configuration options: whether to load or clear the color and
/// depth targets.
static SDL_GPURenderPass *
getRenderPass(const RenderContext &renderer, CommandBuffer &command_buffer,
              SDL_GPULoadOp color_load_op, SDL_GPULoadOp depth_load_op,
              bool has_normals_target, const RobotScene::GBuffer &gbuffer) {
  SDL_GPUColorTargetInfo color_targets[2];
  SDL_zero(color_targets);
  color_targets[0].texture = renderer.swapchain;
  color_targets[0].load_op = color_load_op;
  color_targets[0].store_op = SDL_GPU_STOREOP_STORE;
  color_targets[0].cycle = false;

  SDL_GPUDepthStencilTargetInfo depth_target;
  SDL_zero(depth_target);
  depth_target.texture = renderer.depth_texture;
  depth_target.clear_depth = 1.0f;
  depth_target.load_op = depth_load_op;
  depth_target.store_op = SDL_GPU_STOREOP_STORE;
  depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  if (has_normals_target) {
    color_targets[1].texture = gbuffer.normalMap;
    color_targets[1].load_op = SDL_GPU_LOADOP_CLEAR;
    color_targets[1].store_op = SDL_GPU_STOREOP_STORE;
    color_targets[1].cycle = false;
  }
  Uint32 num_color_targets = has_normals_target ? 2 : 1;
  return SDL_BeginGPURenderPass(command_buffer, color_targets,
                                num_color_targets, &depth_target);
}

static SDL_GPURenderPass *
getTransparentRenderPass(const RenderContext &renderer,
                         CommandBuffer &command_buffer,
                         const RobotScene::GBuffer &gBuffer) {
  SDL_GPUColorTargetInfo targets[2];
  SDL_zero(targets);
  targets[0].texture = gBuffer.accumTexture;
  targets[0].load_op = SDL_GPU_LOADOP_CLEAR;
  targets[0].store_op = SDL_GPU_STOREOP_STORE;

  targets[1].texture = gBuffer.revealTexture;
  targets[1].clear_color = {1, 0, 0, 0}; // clear to red, since it's R8
  targets[1].load_op = SDL_GPU_LOADOP_CLEAR;
  targets[1].store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPUDepthStencilTargetInfo depth_target;
  SDL_zero(depth_target);
  depth_target.texture = renderer.depth_texture;
  depth_target.load_op = SDL_GPU_LOADOP_LOAD;
  depth_target.store_op = SDL_GPU_STOREOP_STORE;

  return SDL_BeginGPURenderPass(command_buffer, targets, 2, &depth_target);
}

void RobotScene::compositeTransparencyPass(CommandBuffer &command_buffer) {
  // transparent triangle pipeline required
  if (!m_pipelines.triangleMeshTransparent || !m_pipelines.wboitComposite)
    return;

  SDL_GPUColorTargetInfo target;
  SDL_zero(target);
  target.texture = m_renderer.swapchain;
  // op is LOAD - we want to keep results from opaque pass
  target.load_op = SDL_GPU_LOADOP_LOAD;
  target.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, &target, 1, nullptr);
  SDL_BindGPUGraphicsPipeline(render_pass, m_pipelines.wboitComposite);

  // Bind accumulation and revealage textures
  rend::bindFragmentSamplers(
      render_pass, 0,
      {
          {.texture = gBuffer.accumTexture, .sampler = gBuffer.sampler},
          {.texture = gBuffer.revealTexture, .sampler = gBuffer.sampler},
      });

  // Draw fullscreen quad
  SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);

  SDL_EndGPURenderPass(render_pass);
}

void RobotScene::renderPBRTriangleGeometry(CommandBuffer &command_buffer,
                                           const Camera &camera,
                                           bool transparent) {

  auto *pipeline = routePipeline(PIPELINE_TRIANGLEMESH, transparent);
  if (!pipeline) {
    return;
  }

  const Uint32 numLights = shadowPass.numLights();
  // calculate light ubos
  LightArrayUbo lightUbo;
  lightUbo.numLights = numLights;
  for (size_t i = 0; i < lightUbo.numLights; i++) {
    auto &dl = directionalLight[i];
    lightUbo.viewSpaceDir[i].head<3>() = camera.transformVector(dl.direction);
    lightUbo.color[i].head<3>() = dl.color;
    lightUbo.intensity[i].x() = dl.intensity;
  }

  const bool enable_shadows = m_config.enable_shadows;
  ShadowAtlasInfoUbo shadowAtlasUbo{
      .regions{},
  };
  Mat4f lightViewProj[kNumLights];
  for (size_t i = 0; i < numLights; i++) {
    lightViewProj[i].noalias() = shadowPass.cam[i].viewProj();
    const auto &reg = shadowPass.regions[i];
    shadowAtlasUbo.regions[i] = {reg.x, reg.y, reg.w, reg.h};
  }
  const Mat4f viewProj = camera.viewProj();

  // this is the first render pass, hence:
  // clear the color texture (swapchain), either load or clear the depth texture
  SDL_GPURenderPass *render_pass =
      transparent
          ? getTransparentRenderPass(m_renderer, command_buffer, gBuffer)
          : getRenderPass(m_renderer, command_buffer, SDL_GPU_LOADOP_CLEAR,
                          m_config.triangle_has_prepass ? SDL_GPU_LOADOP_LOAD
                                                        : SDL_GPU_LOADOP_CLEAR,
                          m_config.enable_normal_target, gBuffer);

  if (enable_shadows) {
    rend::bindFragmentSamplers(render_pass, SHADOW_MAP_SLOT,
                               {{
                                   .texture = shadowPass.shadowMap,
                                   .sampler = shadowPass.sampler,
                               }});
  }
  rend::bindFragmentSamplers(render_pass, SSAO_SLOT,
                             {{
                                 .texture = ssaoPass.ssaoMap,
                                 .sampler = ssaoPass.texSampler,
                             }});
  int _useSsao = m_config.enable_ssao;
  command_buffer //
      .pushFragmentUniform(FragmentUniformSlots::LIGHTING, lightUbo)
      .pushFragmentUniform(FragmentUniformSlots::SSAO_FLAG, _useSsao)
      .pushFragmentUniform(FragmentUniformSlots::ATLAS_INFO, shadowAtlasUbo);

  SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

  auto view =
      m_registry.view<const TransformComponent, const MeshMaterialComponent,
                      pipeline_tag<PIPELINE_TRIANGLEMESH>>(
          entt::exclude<Disable>);

  auto process_entities = [&](const TransformComponent &tr, const auto &obj) {
    const Mat4f modelView = camera.view * tr;
    const Mesh &mesh = obj.mesh;
    Mat4f mvp = viewProj * tr;
    TransformUniformData data{
        .modelView = modelView,
        .mvp = mvp,
        .normalMatrix = math::computeNormalMatrix(modelView),
    };
    command_buffer.pushVertexUniform(VertexUniformSlots::TRANSFORM, data);
    if (enable_shadows) {
      LightSpaceMatricesUbo shadowUbo;
      shadowUbo.numLights = numLights;
      for (size_t i = 0; i < numLights; i++) {
        GpuMat4 lightMvp = lightViewProj[i] * tr;
        shadowUbo.mvps[i] = lightMvp;
      }
      command_buffer.pushVertexUniform(VertexUniformSlots::LIGHT_MATRICES,
                                       shadowUbo);
    }
    rend::bindMesh(render_pass, mesh);
    for (size_t j = 0; j < mesh.numViews(); j++) {
      command_buffer.pushFragmentUniform(FragmentUniformSlots::MATERIAL,
                                         obj.materials[j]);
      rend::drawView(render_pass, mesh.view(j));
    }
  };

  if (transparent) {
    (view | m_registry.view<entt::entity>(entt::exclude<Opaque>))
        .each(process_entities);
  } else {
    (view | m_registry.view<Opaque>()).each(process_entities);
  }

  SDL_EndGPURenderPass(render_pass);
}

void RobotScene::renderOtherGeometry(CommandBuffer &command_buffer,
                                     const Camera &camera) {
  SDL_GPURenderPass *render_pass =
      getRenderPass(m_renderer, command_buffer, SDL_GPU_LOADOP_LOAD,
                    SDL_GPU_LOADOP_LOAD, false, gBuffer);

  const Mat4f viewProj = camera.viewProj();

  // iterate over primitive types in the keys
  magic_enum::enum_for_each<PipelineType>([&](auto current_pipeline_type) {
    if (current_pipeline_type == PIPELINE_TRIANGLEMESH)
      return;

    auto *pipeline = routePipeline(current_pipeline_type);
    if (!pipeline)
      return;
    SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

    auto env_view =
        m_registry.view<const TransformComponent, const MeshMaterialComponent,
                        pipeline_tag<current_pipeline_type>>(
            entt::exclude<Disable>);
    for (auto &&[entity, tr, obj] : env_view.each()) {
      const Mesh &mesh = obj.mesh;
      const GpuMat4 mvp = viewProj * tr;
      const auto &color = obj.materials[0].baseColor;
      command_buffer.pushVertexUniform(VertexUniformSlots::TRANSFORM, mvp)
          .pushFragmentUniform(FragmentUniformSlots::MATERIAL, color);
      rend::bindMesh(render_pass, mesh);
      rend::draw(render_pass, mesh);
    }
  });
  SDL_EndGPURenderPass(render_pass);
}

void RobotScene::release() {
  if (!device())
    return;

  clearEnvironment();
  clearRobotGeometries();

  // release pipelines
  for (auto **pipe :
       {&m_pipelines.triangleMeshOpaque, &m_pipelines.triangleMeshTransparent,
        &m_pipelines.heightfield, &m_pipelines.pointcloud,
        &m_pipelines.wboitComposite}) {
    if (*pipe)
      SDL_ReleaseGPUGraphicsPipeline(device(), *pipe);
    *pipe = nullptr;
  }

  gBuffer.normalMap.destroy();
  gBuffer.accumTexture.destroy();
  gBuffer.revealTexture.destroy();
  if (gBuffer.sampler) {
    SDL_ReleaseGPUSampler(device(), gBuffer.sampler);
    gBuffer.sampler = nullptr;
  }
  ssaoPass.release();
  shadowPass.release();
}

static RobotScene::PipelineConfig
get_pipeline_config(const RobotScene::Config &cfg,
                    RobotScene::PipelineType type, bool transparent) {
  using enum RobotScene::PipelineType;
  switch (type) {
  case PIPELINE_TRIANGLEMESH:
    return transparent ? cfg.triangle_config.transparent
                       : cfg.triangle_config.opaque;
  case PIPELINE_HEIGHTFIELD:
    return cfg.heightfield_config;
  case PIPELINE_POINTCLOUD:
    return cfg.pointcloud_config;
  }
}

void RobotScene::createRenderPipeline(const MeshLayout &layout,
                                      SDL_GPUTextureFormat render_target_format,
                                      SDL_GPUTextureFormat depth_stencil_format,
                                      PipelineType type, bool transparent) {
  assert(validateMeshLayout(layout));

  SDL_Log("Building pipeline for type %s", magic_enum::enum_name(type).data());

  PipelineConfig pipe_config = get_pipeline_config(m_config, type, transparent);
  auto vertexShader =
      Shader::fromMetadata(device(), pipe_config.vertex_shader_path);
  auto fragmentShader =
      Shader::fromMetadata(device(), pipe_config.fragment_shader_path);

  SDL_GPUColorTargetDescription color_targets[2];
  SDL_zero(color_targets);
  color_targets[0].format = render_target_format;
  color_targets[0].blend_state = {
      .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
      .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
      .color_blend_op = SDL_GPU_BLENDOP_ADD,
      .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
      .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
      .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
      .enable_blend = true,
  };
  bool had_prepass =
      (type == PIPELINE_TRIANGLEMESH) && m_config.triangle_has_prepass;
  SDL_GPUCompareOp depth_compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

  SDL_GPUGraphicsPipelineCreateInfo desc{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
      .vertex_input_state = layout,
      .primitive_type = getPrimitiveTopologyForType(type),
      .depth_stencil_state{
          .compare_op = depth_compare_op,
          .enable_depth_test = true,
          // no depth write if there was a prepass
          .enable_depth_write = !had_prepass,
      },
      .target_info{
          .color_target_descriptions = color_targets,
          .num_color_targets = 1,
          .depth_stencil_format = depth_stencil_format,
          .has_depth_stencil_target = true,
      },
  };

  if (type == PIPELINE_TRIANGLEMESH && m_config.enable_normal_target) {
    color_targets[1].format = gBuffer.normalMap.format();
    desc.target_info.num_color_targets = 2;
  }

  if (type == PIPELINE_TRIANGLEMESH && transparent) {
    // modify color targets
    SDL_zero(color_targets);
    // Accumulation target with additive blending
    color_targets[0].format = gBuffer.accumTexture.format();
    color_targets[0].blend_state = {
        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
        .color_blend_op = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .enable_blend = true,
    };

    // Revealage target with multiplicative blending
    color_targets[1].format = gBuffer.revealTexture.format();
    color_targets[1].blend_state = {
        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR,
        .color_blend_op = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .enable_blend = true,
    };

    desc.target_info.num_color_targets = 2;
    desc.depth_stencil_state.enable_depth_write = false;
  }

  if (type == PIPELINE_TRIANGLEMESH) {
    SDL_Log("  > transparency: %s", transparent ? "true" : "false");
    SDL_Log("  > depth compare op: %s",
            magic_enum::enum_name(depth_compare_op).data());
    SDL_Log("  > prepass: %s", had_prepass ? "true" : "false");
  }
  desc.rasterizer_state.cull_mode = pipe_config.cull_mode;
  desc.rasterizer_state.fill_mode = pipe_config.fill_mode;
  routePipeline(type, transparent) =
      SDL_CreateGPUGraphicsPipeline(device(), &desc);
}

} // namespace candlewick::multibody
