#include "DepthAndShadowPass.h"
#include "RenderContext.h"
#include "Shader.h"
#include "Collision.h"
#include "Camera.h"

#include <SDL3/SDL_log.h>

namespace candlewick {

static SDL_GPUGraphicsPipeline *
create_depth_pass_pipeline(const Device &device, const MeshLayout &layout,
                           SDL_GPUTextureFormat format,
                           const DepthPass::Config config) {
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
  return SDL_CreateGPUGraphicsPipeline(device, &pipeline_desc);
}

DepthPass::DepthPass(const Device &device, const MeshLayout &layout,
                     SDL_GPUTexture *depth_texture, SDL_GPUTextureFormat format,
                     const Config &config)
    : _device(device), depthTexture(depth_texture) {
  pipeline = create_depth_pass_pipeline(device, layout, format, config);
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

static SDL_GPUViewport
gpuViewportFromAtlasRegion(const ShadowMapPass::AtlasRegion &reg) {
  return {float(reg.x), float(reg.y), float(reg.w), float(reg.h), 0.f, 1.f};
};

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

void ShadowMapPass::configureAtlasRegions(const Config &config) {
  SDL_Log("Building shadow atlas.\n"
          "  > Dims: (%d, %d)\n"
          "  > %d regions:",
          shadowMap.width(), shadowMap.height(), config.numLights);

  // compute atlas regions
  for (Uint32 i = 0; i < config.numLights; i++) {
    regions[i] = AtlasRegion(i * config.width, 0, config.width, config.height);
    const auto &reg = regions[i];
    regions[i] = reg;
    SDL_Log("    - %d: [%d, %d] x [%d, %d]", i, reg.x, reg.x + reg.w, reg.y,
            reg.y + reg.h);
  }
}

ShadowMapPass::ShadowMapPass(const Device &device, const MeshLayout &layout,
                             SDL_GPUTextureFormat format, const Config &config)
    : _device(device), _numLights(config.numLights) {

  const Uint32 atlasWidth = config.numLights * config.width;
  const Uint32 atlasHeight = config.height;
  // TEXTURE
  // 2k x 2k texture
  SDL_GPUTextureCreateInfo texInfo{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = format,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
               SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = atlasWidth,
      .height = atlasHeight,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
      .props = 0,
  };

  this->configureAtlasRegions(config);

  shadowMap = Texture(device, texInfo, "Shadow atlas");
  pipeline = create_depth_pass_pipeline(device, layout, format,
                                        {
                                            SDL_GPU_CULLMODE_FRONT,
                                            config.depth_bias_constant_factor,
                                            config.depth_bias_slope_factor,
                                            config.enable_depth_bias,
                                            config.enable_depth_clip,
                                        });
  if (!pipeline)
    terminate_with_message("Failed to create shadow cast pipeline: {:s}.",
                           SDL_GetError());

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
    , _numLights(other._numLights)
    , shadowMap(std::move(other.shadowMap))
    , pipeline(other.pipeline)
    , sampler(other.sampler)
    , cam(std::move(other.cam))
    , regions(std::move(other.regions)) {
  other._device = nullptr;
  other.pipeline = nullptr;
  other.sampler = nullptr;
}

ShadowMapPass &ShadowMapPass::operator=(ShadowMapPass &&other) noexcept {
  _device = other._device;
  _numLights = other._numLights;
  shadowMap = std::move(other.shadowMap);
  sampler = other.sampler;
  pipeline = other.pipeline;
  cam = std::move(other.cam);
  regions = std::move(other.regions);

  other._device = nullptr;
  other.pipeline = nullptr;
  other.sampler = nullptr;
  return *this;
}

void ShadowMapPass::release() noexcept {
  if (_device) {
    if (sampler) {
      SDL_ReleaseGPUSampler(_device, sampler);
    }
    sampler = nullptr;
    if (pipeline) {
      SDL_ReleaseGPUGraphicsPipeline(_device, pipeline);
    }
    pipeline = nullptr;
  }
  shadowMap.destroy();
  _device = nullptr;
}

void ShadowMapPass::render(CommandBuffer &command_buffer,
                           std::span<const OpaqueCastable> castables) {
  SDL_GPUDepthStencilTargetInfo depth_info{
      .texture = shadowMap,
      .clear_depth = 1.0f,
      .load_op = SDL_GPU_LOADOP_CLEAR,
      .store_op = SDL_GPU_STOREOP_STORE,
      .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
      .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
  };

  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(command_buffer, nullptr, 0, &depth_info);
  SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

  for (size_t i = 0; i < numLights(); i++) {
    SDL_GPUViewport vp = gpuViewportFromAtlasRegion(regions[i]);
    SDL_SetGPUViewport(render_pass, &vp);

    const Mat4f viewProj = cam[i].viewProj();

    for (auto &[mesh, tr] : castables) {
      assert(validateMesh(mesh));
      rend::bindMesh(render_pass, mesh);

      GpuMat4 mvp = viewProj * tr;
      command_buffer.pushVertexUniform(0, mvp);
      rend::draw(render_pass, mesh);
    }
  }

  SDL_EndGPURenderPass(render_pass);
}

void renderShadowPassFromFrustum(CommandBuffer &cmdBuf, ShadowMapPass &passInfo,
                                 std::span<const DirectionalLight> dirLight,
                                 std::span<const OpaqueCastable> castables,
                                 const FrustumCornersType &worldSpaceCorners) {
  auto [center, radius] = frustumBoundingSphereCenterRadius(worldSpaceCorners);

  for (size_t i = 0; i < passInfo.numLights(); i++) {
    Float3 eye = center - radius * dirLight[i].direction.normalized();
    Mat4f tmpLightView = lookAt(eye, center, Float3::UnitZ());
    // transform corners to light-space
    FrustumCornersType corners = worldSpaceCorners;
    frustumApplyTransform(corners, tmpLightView);

    coal::AABB bounds;
    for (const auto &c : corners) {
      bounds += c.cast<coal::CoalScalar>();
    }
    float r = float(bounds.max_.z());

    auto &lightView = passInfo.cam[i].view;
    auto &lightProj = passInfo.cam[i].projection;
    eye = center - r * dirLight[i].direction.normalized();
    lightView = lookAt(eye, center, Float3::UnitZ());

    lightProj = shadowOrthographicMatrix({bounds.width(), bounds.height()},
                                         float(bounds.max_.z()),
                                         float(bounds.min_.z()));
  }
  passInfo.render(cmdBuf, castables);
}

void renderShadowPassFromAABB(CommandBuffer &cmdBuf, ShadowMapPass &passInfo,
                              std::span<const DirectionalLight> dirLight,
                              std::span<const OpaqueCastable> castables,
                              const AABB &worldAABB) {
  Float3 center = worldAABB.center().cast<float>();

  for (size_t i = 0; i < passInfo.numLights(); i++) {
    Float3 eye = center - 100.0f * dirLight[i].direction.normalized();
    Mat4f tmpLightView = lookAt(eye, center, Float3::UnitZ());
    AABB bounds = applyTransformToAABB(worldAABB, tmpLightView);

    auto &lightView = passInfo.cam[i].view;
    auto &lightProj = passInfo.cam[i].projection;

    float radius = float(bounds.max_.z());

    eye = center - radius * dirLight[i].direction.normalized();

    lightView = lookAt(eye, center, Float3::UnitZ());
    lightProj = shadowOrthographicMatrix({bounds.width(), bounds.height()},
                                         float(bounds.max_.z()),
                                         float(bounds.min_.z()));
  }
  passInfo.render(cmdBuf, castables);
}
} // namespace candlewick
