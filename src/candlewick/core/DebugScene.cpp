#include "DebugScene.h"
#include "Camera.h"
#include "Shader.h"
#include "Components.h"

#include "../primitives/Arrow.h"
#include "../primitives/Grid.h"

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

namespace candlewick {

DebugScene::DebugScene(entt::registry &reg, const RenderContext &renderer)
    : m_registry(reg)
    , m_renderer(renderer)
    , m_trianglePipeline(NoInit)
    , m_linePipeline(NoInit)
    , m_sharedMeshes() {
  this->initializeSharedMeshes();
}

DebugScene::DebugScene(DebugScene &&other)
    : m_registry(other.m_registry)
    , m_renderer(other.m_renderer)
    , m_trianglePipeline(std::move(other.m_trianglePipeline))
    , m_linePipeline(std::move(other.m_linePipeline))
    , m_subsystems(std::move(other.m_subsystems))
    , m_sharedMeshes(std::move(other.m_sharedMeshes)) {}

void DebugScene::initializeSharedMeshes() {
  {
    std::array triad_datas = loadTriadSolid();
    Mesh mesh = createMeshFromBatch(device(), triad_datas, true);
    setupPipelines(mesh.layout());
    m_sharedMeshes.emplace(TRIAD, std::move(mesh));
  }
  m_sharedMeshes.emplace(GRID, createMesh(device(), loadGrid(20), true));
  m_sharedMeshes.emplace(ARROW,
                         createMesh(device(), loadArrowSolid(false), true));
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

std::tuple<entt::entity, DebugMeshComponent &>
DebugScene::addArrow(const Float4 &color) {
  auto entity = m_registry.create();
  auto &dmc = m_registry.emplace<DebugMeshComponent>(
      entity, DebugPipelines::TRIANGLE_FILL, DebugMeshType::ARROW,
      std::vector{color}, true);
  dmc.scale << 0.333f, 0.333f, 1.0f;
  m_registry.emplace<TransformComponent>(entity, Mat4f::Identity());
  return {entity, dmc};
}

void DebugScene::setupPipelines(const MeshLayout &layout) {
  if (m_linePipeline.initialized() && m_trianglePipeline.initialized())
    return;
  auto vertexShader = Shader::fromMetadata(device(), "Hud3dElement.vert");
  auto fragmentShader = Shader::fromMetadata(device(), "Hud3dElement.frag");
  SDL_GPUColorTargetDescription color_desc;
  SDL_zero(color_desc);
  color_desc.format = m_renderer.colorFormat();
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
      .multisample_state{m_renderer.getMsaaSampleCount()},
      .depth_stencil_state{.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                           .enable_depth_test = true,
                           .enable_depth_write = true},
      .target_info{.color_target_descriptions = &color_desc,
                   .num_color_targets = 1,
                   .depth_stencil_format = m_renderer.depthFormat(),
                   .has_depth_stencil_target = true},
      .props = 0,
  };
  if (!m_trianglePipeline.initialized())
    m_trianglePipeline = GraphicsPipeline(device(), info, "Debug [triangle]");

  // re-use
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
  if (!m_linePipeline.initialized())
    m_linePipeline = GraphicsPipeline(device(), info, "Debug [line]");
}

void DebugScene::render(CommandBuffer &cmdBuf, const Camera &camera) const {

  SDL_GPUColorTargetInfo color_target_info;
  SDL_zero(color_target_info);
  color_target_info.texture = m_renderer.colorTarget();
  color_target_info.load_op = SDL_GPU_LOADOP_LOAD;
  color_target_info.store_op = SDL_GPU_STOREOP_STORE;
  color_target_info.cycle = false;
  // do resolve to the target that's presented to swapchain
  // if (m_renderer.msaaEnabled()) {
  //   color_target_info.resolve_texture = m_renderer.resolvedColorTarget();
  //   color_target_info.store_op = SDL_GPU_STOREOP_RESOLVE_AND_STORE;
  // }
  SDL_GPUDepthStencilTargetInfo depth_target_info;
  SDL_zero(depth_target_info);
  depth_target_info.load_op = SDL_GPU_LOADOP_LOAD;
  depth_target_info.store_op = SDL_GPU_STOREOP_STORE;
  depth_target_info.stencil_load_op = SDL_GPU_LOADOP_LOAD;
  depth_target_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  depth_target_info.texture = m_renderer.depthTarget();
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
      m_trianglePipeline.bind(render_pass);
      break;
    case DebugPipelines::TRIANGLE_LINE:
      m_linePipeline.bind(render_pass);
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
  m_trianglePipeline.release();
  m_linePipeline.release();

  // clean up all DebugMeshComponent objects.
  auto view = m_registry.view<DebugMeshComponent>();
  m_registry.destroy(view.begin(), view.end());
  m_sharedMeshes.clear();
}

namespace gui {
  void addDebugMesh(DebugMeshComponent &dmc, bool enable_pipeline_switch) {
    ImGui::Checkbox("##enabled", &dmc.enable);
    Uint32 col_id = 0;
    ImGuiColorEditFlags color_flags = ImGuiColorEditFlags_NoAlpha |
                                      ImGuiColorEditFlags_NoSidePreview |
                                      ImGuiColorEditFlags_NoInputs;
    char label[32];
    for (auto &col : dmc.colors) {
      SDL_snprintf(label, sizeof(label), "##color##%u", col_id);
      ImGui::SameLine();
      ImGui::ColorEdit4(label, col.data(), color_flags);
      col_id++;
    }
    if (enable_pipeline_switch) {
      const char *names[] = {"FILL", "LINE"};
      static_assert(IM_ARRAYSIZE(names) ==
                    magic_enum::enum_count<DebugPipelines>());
      ImGui::SameLine();
      ImGui::Combo("Mode##pipeline", (int *)&dmc.pipeline_type, names,
                   IM_ARRAYSIZE(names));
    }
  }
} // namespace gui

} // namespace candlewick
