#include "DepthViz.h"
#include "../Renderer.h"
#include "../Shader.h"
#include <format>

namespace candlewick {

DepthDebugPass DepthDebugPass::create(const Renderer &renderer,
                                      SDL_GPUTexture *depthTexture) {
  const auto &device = renderer.device;
  auto vertexShader = Shader::fromMetadata(device, "DrawQuad.vert");
  auto fragmentShader = Shader::fromMetadata(device, "RenderDepth.frag");

  SDL_GPUColorTargetDescription color_target_desc;
  SDL_zero(color_target_desc);
  // render to swapchain
  color_target_desc.format = renderer.getSwapchainTextureFormat();

  /* SAMPLER */
  SDL_GPUSamplerCreateInfo sampler_desc{
      .min_filter = SDL_GPU_FILTER_NEAREST,
      .mag_filter = SDL_GPU_FILTER_NEAREST,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .compare_op = SDL_GPU_COMPAREOP_NEVER,
  };
  SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &sampler_desc);

  /* PIPELINE */
  SDL_GPUGraphicsPipelineCreateInfo pipeline_desc;
  SDL_zero(pipeline_desc);
  pipeline_desc.vertex_shader = vertexShader;
  pipeline_desc.fragment_shader = fragmentShader;
  pipeline_desc.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline_desc.rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
                                    .cull_mode = SDL_GPU_CULLMODE_NONE};
  pipeline_desc.depth_stencil_state = {.enable_depth_test = false,
                                       .enable_depth_write = false};
  pipeline_desc.target_info = {.color_target_descriptions = &color_target_desc,
                               .num_color_targets = 1,
                               .has_depth_stencil_target = false};

  SDL_GPUGraphicsPipeline *pipeline =
      SDL_CreateGPUGraphicsPipeline(device, &pipeline_desc);
  if (!pipeline) {
    auto msg = std::format("Failed to create depth debug pipeline: %s",
                           SDL_GetError());
    throw std::runtime_error(msg);
  }

  return {depthTexture, sampler, pipeline};
}

struct alignas(16) cam_param_ubo_t {
  Sint32 mode;
  float near_plane;
  float far_plane;
  Uint32 is_ortho;
};

void renderDepthDebug(const Renderer &renderer, CommandBuffer &command_buffer,
                      const DepthDebugPass &pass,
                      const DepthDebugPass::Options &opts) {
  SDL_GPUColorTargetInfo color_target;
  SDL_zero(color_target);
  color_target.texture = renderer.swapchain;
  color_target.clear_color = {0., 0., 0., 1.};
  color_target.load_op = SDL_GPU_LOADOP_CLEAR;
  color_target.store_op = SDL_GPU_STOREOP_STORE;
  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, &color_target, 1, nullptr);

  SDL_BindGPUGraphicsPipeline(render_pass, pass.pipeline);

  rend::bindFragmentSamplers(render_pass, 0,
                             {{
                                 .texture = pass.depthTexture,
                                 .sampler = pass.sampler,
                             }});

  cam_param_ubo_t cam_ubo{opts.mode, opts.near, opts.far,
                          opts.cam_proj == CameraProjection::ORTHOGRAPHIC};

  command_buffer.pushFragmentUniformRaw(0, &cam_ubo, sizeof(cam_ubo));
  SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);

  SDL_EndGPURenderPass(render_pass);
}

} // namespace candlewick
