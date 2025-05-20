#include "DepthAndShadowPass.h"
#include "Renderer.h"
#include "Shader.h"
#include "Collision.h"
#include "Camera.h"

#include <format>
#include <SDL3/SDL_log.h>

namespace candlewick {

DepthPass::DepthPass(const Device &device, const MeshLayout &layout,
                     SDL_GPUTexture *depth_texture, SDL_GPUTextureFormat format,
                     const Config &config)
    : _device(device), depthTexture(depth_texture) {
  auto vertexShader = Shader::fromMetadata(device, "ShadowCast.vert");
  auto fragmentShader = Shader::fromMetadata(device, "ShadowCast.frag");
  SDL_GPUGraphicsPipelineCreateInfo pipeline_desc{
      .vertex_shader = vertexShader,
      .fragment_shader = fragmentShader,
      .vertex_input_state = layout.toVertexInputState(),
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .rasterizer_state{
          .fill_mode = SDL_GPU_FILLMODE_FILL,
          .cull_mode = config.cull_mode,
          .depth_bias_constant_factor = config.depth_bias_constant_factor,
          .depth_bias_slope_factor = config.depth_bias_slope_factor,
          .enable_depth_bias = config.enable_depth_bias,
          .enable_depth_clip = config.enable_depth_clip},
      .depth_stencil_state{.compare_op = SDL_GPU_COMPAREOP_LESS,
                           .enable_depth_test = true,
                           .enable_depth_write = true},
      .target_info{.color_target_descriptions = nullptr,
                   .num_color_targets = 0,
                   .depth_stencil_format = format,
                   .has_depth_stencil_target = true},
  };
  pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_desc);
  if (!pipeline)
    terminate_with_message("Failed to create graphics pipeline: {:s}.",
                           SDL_GetError());
}

DepthPass::DepthPass(const Device &device, const MeshLayout &layout,
                     const Texture &texture, const Config &config)
    : DepthPass(device, layout, texture, texture.format(), config) {}

DepthPass::DepthPass(DepthPass &&other) noexcept
    : _device(other._device)
    , depthTexture(other.depthTexture)
    , pipeline(other.pipeline) {
  other._device = nullptr;
  other.depthTexture = nullptr;
  other.pipeline = nullptr;
}

DepthPass &DepthPass::operator=(DepthPass &&other) noexcept {
  if (this != &other) {
    _device = other._device;
    depthTexture = other.depthTexture;
    pipeline = other.pipeline;
    other._device = nullptr;
    other.depthTexture = nullptr;
    other.pipeline = nullptr;
  }
  return *this;
}

void DepthPass::render(CommandBuffer &command_buffer, const Mat4f &viewProj,
                       std::span<const OpaqueCastable> castables) {
  SDL_GPUDepthStencilTargetInfo depth_info{
      .texture = depthTexture,
      .clear_depth = 1.0f,
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE,
      .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
      .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
  };

  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, nullptr, 0, &depth_info);
  SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

  for (auto &[mesh, tr] : castables) {
    assert(validateMesh(mesh));
    rend::bindMesh(render_pass, mesh);

    GpuMat4 mvp = viewProj * tr;
    command_buffer.pushVertexUniform(DepthPass::TRANSFORM_SLOT, mvp);
    rend::draw(render_pass, mesh);
  }

  SDL_EndGPURenderPass(render_pass);
}

void DepthPass::release() noexcept {
  if (_device) {
    if (pipeline) {
      SDL_ReleaseGPUGraphicsPipeline(_device, pipeline);
      pipeline = nullptr;
    }
    if (depthTexture)
      depthTexture = nullptr;
  }
  _device = nullptr;
}

ShadowMapPass::ShadowMapPass(const Device &device, const MeshLayout &layout,
                             SDL_GPUTextureFormat format, const Config &config)
    : _device(device) {

  // TEXTURE
  // 2k x 2k texture
  SDL_GPUTextureCreateInfo texInfo{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = format,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
               SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = config.width,
      .height = config.height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
      .props = 0,
  };

  shadowMap = Texture(device, texInfo, "Shadow map");
  _depthPass = DepthPass(device, layout, shadowMap, config.depthPassConfig);

  SDL_GPUSamplerCreateInfo sample_desc{
      .min_filter = SDL_GPU_FILTER_LINEAR,
      .mag_filter = SDL_GPU_FILTER_LINEAR,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .compare_op = SDL_GPU_COMPAREOP_LESS,
      .enable_compare = true,
  };
  sampler = SDL_CreateGPUSampler(device, &sample_desc);
}

ShadowMapPass::ShadowMapPass(ShadowMapPass &&other) noexcept
    : _device(other._device)
    , _depthPass(std::move(other._depthPass))
    , shadowMap(std::move(other.shadowMap))
    , sampler(other.sampler) {
  other._device = nullptr;
  other.sampler = nullptr;
}

ShadowMapPass &ShadowMapPass::operator=(ShadowMapPass &&other) noexcept {
  _device = other._device;
  _depthPass = std::move(other._depthPass);
  shadowMap = std::move(other.shadowMap);
  sampler = other.sampler;

  other._device = nullptr;
  other.sampler = nullptr;
  return *this;
}

void ShadowMapPass::release() noexcept {
  _depthPass.release();
  if (_device) {
    if (sampler) {
      SDL_ReleaseGPUSampler(_device, sampler);
    }
    sampler = nullptr;
  }
  shadowMap.destroy();
  _device = nullptr;
}

void renderShadowPassFromFrustum(CommandBuffer &cmdBuf, ShadowMapPass &passInfo,
                                 const DirectionalLight &dirLight,
                                 std::span<const OpaqueCastable> castables,
                                 const FrustumCornersType &worldSpaceCorners) {

  auto [frustumCenter, radius] =
      frustumBoundingSphereCenterRadius(worldSpaceCorners);
  radius = std::ceil(radius * 16.f) / 16.f;

  auto &lightView = passInfo.cam.view;
  auto &lightProj = passInfo.cam.projection;

  const Float3 eye = frustumCenter - radius * dirLight.direction.normalized();
  using Eigen::Vector3d;

  AABB bounds{Vector3d::Constant(-radius), Vector3d::Constant(radius)};
  lightView = lookAt(eye, frustumCenter);
  lightProj =
      shadowOrthographicMatrix({bounds.width(), bounds.height()},
                               float(bounds.min_.z()), float(bounds.max_.z()));

  Mat4f viewProj = lightView * lightProj;
  passInfo.render(cmdBuf, viewProj, castables);
}

void renderShadowPassFromAABB(CommandBuffer &cmdBuf, ShadowMapPass &passInfo,
                              const DirectionalLight &dirLight,
                              std::span<const OpaqueCastable> castables,
                              const AABB &worldSceneBounds) {
  using Eigen::Vector3d;

  Float3 center = worldSceneBounds.center().cast<float>();
  float radius = 0.5f * float(worldSceneBounds.size());
  radius = std::ceil(radius * 16.f) / 16.f;
  Float3 eye = center - radius * dirLight.direction.normalized();
  auto &lightView = passInfo.cam.view;
  auto &lightProj = passInfo.cam.projection;
  lightView = lookAt(eye, center, Float3::UnitZ());

  AABB bounds{Vector3d::Constant(-radius), Vector3d::Constant(radius)};
  lightProj =
      shadowOrthographicMatrix({bounds.width(), bounds.height()},
                               float(bounds.min_.z()), float(bounds.max_.z()));

  Mat4f viewProj = lightProj * lightView.matrix();
  passInfo.render(cmdBuf, viewProj, castables);
}
} // namespace candlewick
