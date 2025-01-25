#include "DepthAndShadowPass.h"
#include "Renderer.h"
#include "Shader.h"
#include "OBB.h"
#include "CameraControl.h"

#include <stdexcept>
#include <format>

namespace candlewick {

DepthPassInfo DepthPassInfo::create(const Renderer &renderer,
                                    const MeshLayout &layout,
                                    SDL_GPUTexture *depth_texture) {
  if (depth_texture == nullptr)
    depth_texture = renderer.depth_texture;
  const Device &device = renderer.device;
  auto vertexShader = Shader::fromMetadata(device, "ShadowCast.vert");
  auto fragmentShader = Shader::fromMetadata(device, "ShadowCast.frag");
  SDL_GPUGraphicsPipelineCreateInfo pipeline_desc{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
      .vertex_input_state = layout.toVertexInputState(),
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state{.fill_mode = SDL_GPU_FILLMODE_FILL,
                        .cull_mode = SDL_GPU_CULLMODE_BACK},
      .depth_stencil_state{.compare_op = SDL_GPU_COMPAREOP_LESS,
                           .enable_depth_test = true,
                           .enable_depth_write = true},
      .target_info{.color_target_descriptions = nullptr,
                   .num_color_targets = 0,
                   .depth_stencil_format = renderer.depth_format,
                   .has_depth_stencil_target = true},
  };
  auto *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_desc);
  return {depth_texture, pipeline};
}

void DepthPassInfo::release(SDL_GPUDevice *device) {
  if (depthTexture) {
    SDL_ReleaseGPUTexture(device, depthTexture);
    depthTexture = nullptr;
  }
  if (pipeline) {
    SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    pipeline = nullptr;
  }
}

ShadowPassInfo ShadowPassInfo::create(const Renderer &renderer,
                                      const MeshLayout &layout,
                                      const ShadowPassConfig &config) {
  const Device &device = renderer.device;

  // TEXTURE
  // 2k x 2k texture
  SDL_GPUTextureCreateInfo texInfo{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = renderer.depth_format,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
               SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = config.width,
      .height = config.height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
      .props = 0,
  };

  SDL_GPUTexture *shadowMap = SDL_CreateGPUTexture(device, &texInfo);
  SDL_SetGPUTextureName(device, shadowMap, "Shadow map");
  if (!shadowMap) {
    auto msg =
        std::format("Failed to create shadow map texture: %s", SDL_GetError());
    throw std::runtime_error(msg);
  }

  DepthPassInfo passInfo = DepthPassInfo::create(renderer, layout, shadowMap);
  if (!passInfo.pipeline) {
    SDL_ReleaseGPUTexture(device, shadowMap);
    auto msg =
        std::format("Failed to create shadow map pipeline: %s", SDL_GetError());
    throw std::runtime_error(msg);
  }

  SDL_GPUSamplerCreateInfo sample_desc{
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .compare_op = SDL_GPU_COMPAREOP_LESS,
      .enable_compare = true,
  };
  return ShadowPassInfo{passInfo, SDL_CreateGPUSampler(device, &sample_desc)};
}

void ShadowPassInfo::release(SDL_GPUDevice *device) {
  DepthPassInfo::release(device);
  if (sampler) {
    SDL_ReleaseGPUSampler(device, sampler);
    sampler = nullptr;
  }
}

void renderDepthOnlyPass(Renderer &renderer, const DepthPassInfo &passInfo,
                         const Mat4f &viewProj,
                         std::span<const OpaqueCastable> castables) {
  SDL_GPUDepthStencilTargetInfo depth_info;
  SDL_zero(depth_info);
  depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
  depth_info.store_op = SDL_GPU_STOREOP_STORE;
  depth_info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
  depth_info.clear_depth = 1.0f;
  // depth texture may be the renderer shared depth texture,
  // or a specially created texture.
  depth_info.texture = passInfo.depthTexture;

  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(renderer.command_buffer, nullptr, 0, &depth_info);

  assert(passInfo.pipeline);
  SDL_BindGPUGraphicsPipeline(render_pass, passInfo.pipeline);

  GpuMat4 mvp;
  for (auto &cs : castables) {
    assert(validateMeshView(cs.mesh));
    renderer.bindMeshView(render_pass, cs.mesh);
    mvp.noalias() = viewProj * cs.transform;
    renderer.pushVertexUniform(DepthPassInfo::TRANSFORM_SLOT, &mvp,
                               sizeof(mvp));
    renderer.drawView(render_pass, cs.mesh);
  }

  SDL_EndGPURenderPass(render_pass);
}

void renderShadowPassFromFrustum(Renderer &renderer,
                                 const ShadowPassInfo &passInfo,
                                 const DirectionalLight &dirLight,
                                 std::span<const OpaqueCastable> castables,
                                 const FrustumCornersType &worldSpaceCorners) {
  Float3 eye = -dirLight.direction;
  Mat4f lightView = lookAt(eye, Float3::Zero());
  Mat4f lightProj;
  orthoProjFromWorldFrustum(lightView, worldSpaceCorners, lightProj);
  renderDepthOnlyPass(renderer, passInfo, lightProj * lightView, castables);
}

void renderShadowPass(Renderer &renderer, const ShadowPassInfo &passInfo,
                      const DirectionalLight &dirLight,
                      std::span<const OpaqueCastable> castables,
                      const AABB &worldSceneBounds) {
  Float3 eye = -dirLight.direction;
  Mat4f lightView = lookAt(eye, Float3::Zero());
  OBB viewSpaceBounds = OBB::fromAABB(worldSceneBounds).transform(lightView);
  Mat4f lightProj = orthoFromAABB(viewSpaceBounds.toAabb());
  Mat4f viewProj = lightProj * lightView;
  renderDepthOnlyPass(renderer, passInfo, viewProj, castables);
}
} // namespace candlewick
