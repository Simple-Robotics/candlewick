#include "Frustum.h"

#include "../RenderContext.h"
#include "../Shader.h"
#include "../Camera.h"

namespace candlewick {
namespace frustum_debug {

  GraphicsPipeline createFrustumDebugPipeline(const RenderContext &renderer) {
    const auto &device = renderer.device;
    auto vertexShader = Shader::fromMetadata(device, "FrustumDebug.vert");
    auto fragmentShader = Shader::fromMetadata(device, "VertexColor.frag");

    SDL_GPUColorTargetDescription color_target;
    SDL_zero(color_target);
    color_target.format = renderer.getSwapchainTextureFormat();

    return GraphicsPipeline(
        device,
        {
            .vertex_shader = vertexShader,
            .fragment_shader = fragmentShader,
            .primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST,
            .depth_stencil_state{
                .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
                .enable_depth_test = true,
                .enable_depth_write = true,
            },
            .target_info{
                .color_target_descriptions = &color_target,
                .num_color_targets = 1,
                .depth_stencil_format = renderer.depthFormat(),
                .has_depth_stencil_target = true,
            },
        },
        "Frustum");
  }

  struct alignas(16) ubo_t {
    GpuMat4 invProj;
    alignas(16) GpuMat4 mvp;
    alignas(16) GpuVec4 color;
    GpuVec3 eyePos;
  };

  static constexpr Uint32 NUM_VERTICES = 36u;

  SDL_GPURenderPass *getDefaultRenderPass(const RenderContext &renderer,
                                          CommandBuffer &cmdBuf) {
    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture = renderer.swapchain;
    color_target.load_op = SDL_GPU_LOADOP_LOAD;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth_target;
    SDL_zero(depth_target);
    depth_target.texture = renderer.depth_texture;
    depth_target.load_op = SDL_GPU_LOADOP_LOAD;
    depth_target.store_op = SDL_GPU_STOREOP_STORE;
    depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth_target.cycle = false;

    return SDL_BeginGPURenderPass(cmdBuf, &color_target, 1, &depth_target);
  }

  void renderFrustum(CommandBuffer &cmdBuf, SDL_GPURenderPass *render_pass,
                     const Mat4f &invProj, const Mat4f &mvp,
                     const Float3 eyePos, const Float4 &color) {

    ubo_t ubo{
        invProj,
        mvp,
        color,
        eyePos,
    };
    cmdBuf.pushVertexUniform(0, ubo);

    SDL_DrawGPUPrimitives(render_pass, NUM_VERTICES, 1, 0, 0);
  }

  void renderFrustum(CommandBuffer &cmdBuf, SDL_GPURenderPass *render_pass,
                     const Camera &camera, const Camera &otherCam,
                     const Float4 &color) {
    Mat4f invProj = otherCam.projection.inverse();
    GpuMat4 mvp = camera.viewProj() * otherCam.pose().matrix();
    renderFrustum(cmdBuf, render_pass, invProj, mvp, otherCam.position(),
                  color);
  }

  void renderOBB(CommandBuffer &cmdBuf, SDL_GPURenderPass *render_pass,
                 const Camera &camera, const coal::OBB &obb,
                 const Float4 &color) {
    Mat4f transform = toTransformationMatrix(obb);
    Mat4f mvp = camera.viewProj() * transform;
    Float3 eyePos = obb.center().cast<float>();
    renderFrustum(cmdBuf, render_pass, Mat4f::Identity(), mvp, eyePos, color);
  }

  void renderAABB(CommandBuffer &cmdBuf, SDL_GPURenderPass *render_pass,
                  const Camera &camera, const AABB &aabb, const Float4 &color) {
    Mat4f transform = toTransformationMatrix(aabb);
    Mat4f mvp = camera.viewProj() * transform;
    Float3 eyePos = aabb.center().cast<float>();
    renderFrustum(cmdBuf, render_pass, Mat4f::Identity(), mvp, eyePos, color);
  }

} // namespace frustum_debug

FrustumBoundsDebugSystem::FrustumBoundsDebugSystem(
    entt::registry &registry, const RenderContext &renderer)
    : renderer(renderer)
    , device(renderer.device)
    , pipeline(NoInit)
    , _registry(registry) {
  pipeline = frustum_debug::createFrustumDebugPipeline(renderer);
}

void FrustumBoundsDebugSystem::render(CommandBuffer &cmdBuf,
                                      const Camera &camera) {
  auto &reg = registry();
  SDL_GPURenderPass *render_pass =
      frustum_debug::getDefaultRenderPass(renderer, cmdBuf);

  pipeline.bind(render_pass);

  for (auto [ent, item] : reg.view<const DebugFrustumComponent>().each()) {
    frustum_debug::renderFrustum(cmdBuf, render_pass, camera, *item.otherCam,
                                 item.color);
  }

  for (auto [ent, item] : reg.view<const DebugBoundsComponent>().each()) {
    std::visit(entt::overloaded{
                   [&](const AABB &bounds) {
                     frustum_debug::renderAABB(cmdBuf, render_pass, camera,
                                               bounds, item.color);
                   },
                   [&](const coal::OBB &obb) {
                     frustum_debug::renderOBB(cmdBuf, render_pass, camera, obb,
                                              item.color);
                   },
               },
               item.bounds);
  }

  SDL_EndGPURenderPass(render_pass);
}

} // namespace candlewick
