#include "RobotScene.h"
#include "Components.h"
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

struct alignas(16) light_ubo_t {
  GpuVec3 viewSpaceDir;
  alignas(16) GpuVec3 color;
  float intensity;
  alignas(16) GpuMat4 projMat;
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

static void add_pipeline_tag_component(entt::registry &reg, entt::entity ent,
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
  add_pipeline_tag_component(m_registry, entity, pipe_type);
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

RobotScene::RobotScene(entt::registry &registry, const Renderer &renderer)
    : m_registry(registry)
    , m_renderer(renderer)
    , m_config()
    , m_geomModel(nullptr)
    , m_geomData(nullptr)
    , m_initialized(false) {
  assert(!hasInternalPointers());
}

RobotScene::RobotScene(entt::registry &registry, const Renderer &renderer,
                       const pin::GeometryModel &geom_model,
                       const pin::GeometryData &geom_data, Config config)
    : RobotScene(registry, renderer) {
  m_config = config;
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

void RobotScene::initCompositePipeline() {
  Shader vertexShader = Shader::fromMetadata(device(), "DrawQuad.vert");
  Shader fragmentShader = Shader::fromMetadata(device(), "WBOITComposite.frag");

  SDL_GPUColorTargetDescription color_target;
  SDL_zero(color_target);
  color_target.format = m_renderer.getSwapchainTextureFormat();

  SDL_GPUGraphicsPipelineCreateInfo desc{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
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

  pipelines.wboitComposite = SDL_CreateGPUGraphicsPipeline(device(), &desc);
}

void RobotScene::loadModels(const pin::GeometryModel &geom_model,
                            const pin::GeometryData &geom_data) {
  assert(!(m_initialized || hasInternalPointers()));
  m_geomModel = &geom_model;
  m_geomData = &geom_data;

  this->initGBuffer();

  // initialize render target for GBuffer
  const bool enable_shadows = m_config.enable_shadows;

  for (pin::GeomIndex geom_id = 0; geom_id < geom_model.ngeoms; geom_id++) {

    const auto &geom_obj = geom_model.geometryObjects[geom_id];
    auto meshDatas = loadGeometryObject(geom_obj);
    PipelineType pipeline_type = pinGeomToPipeline(*geom_obj.geometry);
    Mesh mesh = createMeshFromBatch(device(), meshDatas, true);
    SDL_assert(validateMesh(mesh));

    const auto layout = mesh.layout();

    // add entity for this geometry
    entt::entity entity = m_registry.create();
    m_registry.emplace<PinGeomObjComponent>(entity, geom_id);
    m_registry.emplace<TransformComponent>(entity);
    if (pipeline_type != PIPELINE_POINTCLOUD)
      m_registry.emplace<Opaque>(entity);
    const auto &mmc = m_registry.emplace<MeshMaterialComponent>(
        entity, std::move(mesh), extractMaterials(meshDatas));
    bool is_transparent =
        updateTransparencyClassification(m_registry, entity, mmc);
    if (!getPipeline(pipeline_type, is_transparent)) {
      createPipeline(layout, m_renderer.getSwapchainTextureFormat(),
                     m_renderer.depthFormat(), pipeline_type, is_transparent);
    }
    add_pipeline_tag_component(m_registry, entity, pipeline_type);

    if (pipeline_type == PIPELINE_TRIANGLEMESH) {
      if (!ssaoPass.pipeline) {
        ssaoPass = ssao::SsaoPass(m_renderer, layout, gBuffer.normalMap);
      }
      // configure shadow pass
      if (enable_shadows && !shadowPass.pipeline) {
        shadowPass =
            ShadowPassInfo::create(m_renderer, layout, m_config.shadow_config);
      }
    }
  }
  m_initialized = true;
}

void RobotScene::updateTransforms() {
  updateRobotTransforms(m_registry, geomModel(), geomData());
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

void RobotScene::render(CommandBuffer &command_buffer, const Camera &camera) {
  if (m_config.enable_ssao) {
    ssaoPass.render(command_buffer, camera);
  }

  renderPBRTriangleGeometry(command_buffer, camera, false);

  renderPBRTriangleGeometry(command_buffer, camera, true);

  renderOtherGeometry(command_buffer, camera);
}

/// Function private to this translation unit.
/// Utility function to provide a render pass handle
/// with just two configuration options: whether to load or clear the color and
/// depth targets.
static SDL_GPURenderPass *
getRenderPass(const Renderer &renderer, CommandBuffer &command_buffer,
              SDL_GPULoadOp color_load_op, SDL_GPULoadOp depth_load_op,
              bool has_normals_target, const RobotScene::GBuffer &gbuffer) {
  SDL_GPUColorTargetInfo main_color_target;
  SDL_zero(main_color_target);
  main_color_target.texture = renderer.swapchain;
  main_color_target.clear_color = SDL_FColor{0., 0., 0., 0.};
  main_color_target.load_op = color_load_op;
  main_color_target.store_op = SDL_GPU_STOREOP_STORE;
  main_color_target.cycle = false;

  SDL_GPUDepthStencilTargetInfo depth_target;
  SDL_zero(depth_target);
  depth_target.texture = renderer.depth_texture;
  depth_target.clear_depth = 1.0f;
  depth_target.load_op = depth_load_op;
  depth_target.store_op = SDL_GPU_STOREOP_STORE;
  depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  if (!has_normals_target) {
    return SDL_BeginGPURenderPass(command_buffer, &main_color_target, 1,
                                  &depth_target);
  } else {
    SDL_GPUColorTargetInfo color_targets[2];
    SDL_zero(color_targets);
    color_targets[0] = main_color_target;
    color_targets[1].texture = gbuffer.normalMap;
    color_targets[1].clear_color = SDL_FColor{};
    color_targets[1].load_op = SDL_GPU_LOADOP_CLEAR;
    color_targets[1].store_op = SDL_GPU_STOREOP_STORE;
    color_targets[1].cycle = false;
    return SDL_BeginGPURenderPass(command_buffer, color_targets,
                                  SDL_arraysize(color_targets), &depth_target);
  }
}

static SDL_GPURenderPass *
getTransparentRenderPass(const Renderer &renderer,
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

enum FragmentSamplerSlots {
  SHADOW_MAP_SLOT,
  SSAO_SLOT,
};

void RobotScene::compositeTransparencyPass(CommandBuffer &command_buffer) {
  if (!pipelines.wboitComposite)
    return;

  SDL_GPUColorTargetInfo target;
  SDL_zero(target);
  target.texture = m_renderer.swapchain;
  target.load_op =
      SDL_GPU_LOADOP_LOAD; // Don't clear - we want to keep opaque results
  target.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, &target, 1, nullptr);
  SDL_BindGPUGraphicsPipeline(render_pass, pipelines.wboitComposite);

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

  auto *pipeline = pipelines.triangleMesh.opaque;
  if (!pipeline) {
    // skip of no triangle pipeline to use
    SDL_Log("Skipping triangle render pass...");
    return;
  }

  const light_ubo_t lightUbo{
      camera.transformVector(directionalLight.direction),
      directionalLight.color,
      directionalLight.intensity,
      camera.projection,
  };

  const bool enable_shadows = m_config.enable_shadows;
  const Mat4f lightViewProj = shadowPass.cam.viewProj();
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
                                   .texture = shadowPass.depthTexture,
                                   .sampler = shadowPass.sampler,
                               }});
  }
  rend::bindFragmentSamplers(render_pass, SSAO_SLOT,
                             {{
                                 .texture = ssaoPass.ssaoMap,
                                 .sampler = ssaoPass.texSampler,
                             }});
  int _useSsao = m_config.enable_ssao;
  command_buffer
      .pushFragmentUniform(FragmentUniformSlots::LIGHTING, &lightUbo,
                           sizeof(lightUbo))
      .pushFragmentUniform(2, &_useSsao, sizeof(_useSsao));

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
    command_buffer.pushVertexUniform(VertexUniformSlots::TRANSFORM, &data,
                                     sizeof(data));
    if (enable_shadows) {
      Mat4f lightMvp = lightViewProj * tr;
      command_buffer.pushVertexUniform(1, &lightMvp, sizeof(lightMvp));
    }
    rend::bindMesh(render_pass, mesh);
    for (size_t j = 0; j < mesh.numViews(); j++) {
      const auto material = obj.materials[j];
      command_buffer.pushFragmentUniform(FragmentUniformSlots::MATERIAL,
                                         &material, sizeof(material));
      rend::drawView(render_pass, mesh.view(j));
    }
  };

  if (transparent) {
    auto join = view | m_registry.view<entt::entity>(entt::exclude<Opaque>);
    join.each(process_entities);
  } else {
    auto join = view | m_registry.view<Opaque>();
    join.each(process_entities);
  }

  SDL_EndGPURenderPass(render_pass);

  if (transparent)
    compositeTransparencyPass(command_buffer);
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

    auto *pipeline = getPipeline(current_pipeline_type);
    if (!pipeline)
      return;
    SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

    auto env_view =
        m_registry.view<const TransformComponent, const MeshMaterialComponent,
                        pipeline_tag<current_pipeline_type>>(
            entt::exclude<Disable>);
    for (auto &&[entity, tr, obj] : env_view.each()) {
      const Mesh &mesh = obj.mesh;
      const Mat4f mvp = viewProj * tr;
      const auto &color = obj.materials[0].baseColor;
      command_buffer
          .pushVertexUniform(VertexUniformSlots::TRANSFORM, &mvp, sizeof(mvp))
          .pushFragmentUniform(FragmentUniformSlots::MATERIAL, &color,
                               sizeof(color));
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
       {&pipelines.triangleMesh.opaque, &pipelines.triangleMesh.transparent,
        &pipelines.heightfield, &pipelines.pointcloud,
        &pipelines.wboitComposite}) {
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

void RobotScene::createPipeline(const MeshLayout &layout,
                                SDL_GPUTextureFormat render_target_format,
                                SDL_GPUTextureFormat depth_stencil_format,
                                PipelineType type, bool transparent) {
  SDL_assert(validateMeshLayout(layout));

  SDL_Log("Building pipeline for type %s", magic_enum::enum_name(type).data());

  PipelineConfig pipe_config = get_pipeline_config(m_config, type, transparent);
  auto vertexShader =
      Shader::fromMetadata(device(), pipe_config.vertex_shader_path);
  auto fragmentShader =
      Shader::fromMetadata(device(), pipe_config.fragment_shader_path);

  SDL_GPUColorTargetDescription color_targets[2];
  SDL_zero(color_targets);
  color_targets[0] = {
      .format = render_target_format,
      .blend_state =
          {
              .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
              .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
              .color_blend_op = SDL_GPU_BLENDOP_ADD,
              .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
              .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
              .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
              .enable_blend = true,
          },
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
  *routePipeline(type, transparent) =
      SDL_CreateGPUGraphicsPipeline(device(), &desc);
}

} // namespace candlewick::multibody
