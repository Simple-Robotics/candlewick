#include "DebugScene.h"
#include "Camera.h"
#include "Shader.h"
#include "Components.h"

#include "../primitives/Arrow.h"
#include "../primitives/Grid.h"

namespace candlewick {

DebugScene::DebugScene(entt::registry &reg, const RenderContext &renderer)
    : m_registry(reg)
    , m_renderer(renderer)
    , m_trianglePipeline(nullptr)
    , m_linePipeline(nullptr)
    , m_sharedMeshes() {
  this->initializeSharedMeshes();
}

DebugScene::DebugScene(DebugScene &&other)
    : m_registry(other.m_registry)
    , m_renderer(other.m_renderer)
    , m_trianglePipeline(other.m_trianglePipeline)
    , m_linePipeline(other.m_linePipeline)
    , m_subsystems(std::move(other.m_subsystems))
    , m_sharedMeshes(std::move(other.m_sharedMeshes)) {
  other.m_trianglePipeline = nullptr;
  other.m_linePipeline = nullptr;
}

void DebugScene::initializeSharedMeshes() {
  {
    std::array triad_datas = loadTriadSolid();
    Mesh mesh = createMeshFromBatch(device(), triad_datas, true);
    setupPipelines(mesh.layout());
    m_sharedMeshes.emplace(TRIAD, std::move(mesh));
  }
  {
    MeshData grid_data = loadGrid(20);
    m_sharedMeshes.emplace(GRID, createMesh(device(), grid_data, true));
  }
  {
    MeshData arrow_data = loadArrowSolid(false);
    m_sharedMeshes.emplace(ARROW, createMesh(device(), arrow_data, true));
  }
}

std::tuple<entt::entity, DebugMeshComponent &>
DebugScene::addTriad(const Float3 &scale) {
  entt::entity entity = m_registry.create();
  auto &item = m_registry.emplace<DebugMeshComponent>(
      entity, DebugPipelines::TRIANGLE_FILL, TRIAD,
      std::vector<Float4>{m_triadColors.begin(), m_triadColors.end()}, true,
      scale);
  m_registry.emplace<TransformComponent>(entity, Mat4f::Identity());
  return {entity, item};
}

std::tuple<entt::entity, DebugMeshComponent &>
DebugScene::addLineGrid(const Float4 &color) {
  auto entity = m_registry.create();
  auto &item = m_registry.emplace<DebugMeshComponent>(
      entity, DebugPipelines::TRIANGLE_LINE, GRID, std::vector{color});
  m_registry.emplace<TransformComponent>(entity, Mat4f::Identity());
  return {entity, item};
}

entt::entity DebugScene::addArrow(const Float4 &color) {
  auto entity = m_registry.create();
  auto &dmc = m_registry.emplace<DebugMeshComponent>(
      entity, DebugPipelines::TRIANGLE_FILL, DebugMeshType::ARROW,
      std::vector{color}, true);
  dmc.scale << 0.333f, 0.333f, 1.0f;
  m_registry.emplace<TransformComponent>(entity, Mat4f::Identity());
  return entity;
}

void DebugScene::setupPipelines(const MeshLayout &layout) {
  if (m_linePipeline && m_trianglePipeline)
    return;
  auto vertexShader = Shader::fromMetadata(device(), "Hud3dElement.vert");
  auto fragmentShader = Shader::fromMetadata(device(), "Hud3dElement.frag");
  SDL_GPUColorTargetDescription color_desc;
  SDL_zero(color_desc);
  color_desc.format = m_renderer.getSwapchainTextureFormat();
  SDL_GPUGraphicsPipelineCreateInfo info{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
      .vertex_input_state = layout.toVertexInputState(),
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state{.fill_mode = SDL_GPU_FILLMODE_FILL,
                        .cull_mode = SDL_GPU_CULLMODE_NONE,
                        .depth_bias_constant_factor = 0.0001f,
                        .depth_bias_slope_factor = 0.001f,
                        .enable_depth_bias = true,
                        .enable_depth_clip = true},
      .depth_stencil_state{.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                           .enable_depth_test = true,
                           .enable_depth_write = true},
      .target_info{.color_target_descriptions = &color_desc,
                   .num_color_targets = 1,
                   .depth_stencil_format = m_renderer.depthFormat(),
                   .has_depth_stencil_target = true},
      .props = 0,
  };
  if (!m_trianglePipeline)
    m_trianglePipeline = SDL_CreateGPUGraphicsPipeline(device(), &info);

  // re-use
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
  if (!m_linePipeline)
    m_linePipeline = SDL_CreateGPUGraphicsPipeline(device(), &info);
}

void DebugScene::render(CommandBuffer &cmdBuf, const Camera &camera) const {

  SDL_GPUColorTargetInfo color_target_info;
  SDL_zero(color_target_info);
  color_target_info.texture = m_renderer.swapchain;
  color_target_info.load_op = SDL_GPU_LOADOP_LOAD;
  color_target_info.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPUDepthStencilTargetInfo depth_target_info;
  SDL_zero(depth_target_info);
  depth_target_info.load_op = SDL_GPU_LOADOP_LOAD;
  depth_target_info.store_op = SDL_GPU_STOREOP_STORE;
  depth_target_info.stencil_load_op = SDL_GPU_LOADOP_LOAD;
  depth_target_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  depth_target_info.texture = m_renderer.depth_texture;
  depth_target_info.cycle = false;

  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(cmdBuf, &color_target_info, 1, &depth_target_info);

  const Mat4f viewProj = camera.viewProj();

  auto group =
      m_registry.view<const DebugMeshComponent, const TransformComponent>(
          entt::exclude<Disable>);
  group.each([&](const DebugMeshComponent &cmd, auto &tr) {
    if (!cmd.enable)
      return;

    switch (cmd.pipeline_type) {
    case DebugPipelines::TRIANGLE_FILL:
      SDL_BindGPUGraphicsPipeline(render_pass, m_trianglePipeline);
      break;
    case DebugPipelines::TRIANGLE_LINE:
      SDL_BindGPUGraphicsPipeline(render_pass, m_linePipeline);
      break;
    }

    const GpuMat4 mvp = viewProj * tr;
    cmdBuf.pushVertexUniform(TRANSFORM_SLOT, mvp);
    auto &mesh = this->getMesh(cmd.meshType);
    rend::bindMesh(render_pass, mesh);
    for (size_t i = 0; i < mesh.numViews(); i++) {
      const auto &color = cmd.colors[i];
      cmdBuf.pushFragmentUniform(COLOR_SLOT, color);
      rend::drawView(render_pass, mesh.view(i));
    }
  });

  SDL_EndGPURenderPass(render_pass);
}

void DebugScene::release() {
  if (device() && m_trianglePipeline) {
    SDL_ReleaseGPUGraphicsPipeline(device(), m_trianglePipeline);
    m_trianglePipeline = nullptr;
  }
  if (device() && m_linePipeline) {
    SDL_ReleaseGPUGraphicsPipeline(device(), m_linePipeline);
    m_linePipeline = nullptr;
  }
  // clean up all DebugMeshComponent objects.
  auto view = m_registry.view<DebugMeshComponent>();
  m_registry.destroy(view.begin(), view.end());
  m_sharedMeshes.clear();
}
} // namespace candlewick
