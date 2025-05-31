#include "ScreenSpaceShadows.h"

#include "../core/math_types.h"
#include "../core/DepthAndShadowPass.h"
#include "../core/RenderContext.h"
#include "../core/Shader.h"
#include "../core/Camera.h"

#include <SDL3/SDL_log.h>

namespace candlewick {
namespace effects {
  ScreenSpaceShadowPass::ScreenSpaceShadowPass(const RenderContext &renderer,
                                               const Config &config)
      : config(config) {
    const Device &device = renderer.device;
    this->depthTexture = renderer.depth_texture;

    auto vertexShader = Shader::fromMetadata(device, "ShadowCast.vert");
    auto fragmentShader =
        Shader::fromMetadata(device, "ScreenSpaceShadows.frag");

    auto outputAttachmentFormat = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    color_target_desc.format = outputAttachmentFormat;
    SDL_GPUGraphicsPipelineCreateInfo pipeline_desc{
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
        .vertex_input_state{},
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state{.fill_mode = SDL_GPU_FILLMODE_FILL,
                          .cull_mode = SDL_GPU_CULLMODE_BACK},
        .target_info{.color_target_descriptions = &color_target_desc,
                     .num_color_targets = 1,
                     .has_depth_stencil_target = false},
        .props = 0,
    };

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_desc);
    assert(pipeline);

    auto [width, height] = renderer.window.sizeInPixels();
    SDL_GPUTextureCreateInfo texture_desc{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = outputAttachmentFormat,
        .usage =
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = Uint32(width),
        .height = Uint32(height),
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0,
    };
    targetTexture = SDL_CreateGPUTexture(device, &texture_desc);
    assert(targetTexture);

    SDL_GPUSamplerCreateInfo sampler_desc{};
    sampler_desc.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_desc.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_desc.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_desc.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_desc.enable_compare = false;
    sampler_desc.enable_anisotropy = false;
    sampler_desc.props = 0;
    depthSampler = SDL_CreateGPUSampler(device, &sampler_desc);
    assert(depthSampler);
  }

  struct alignas(16) ScreenSpaceShadowsUniform {
    GpuMat4 projection;
    GpuMat4 invProj;
    GpuVec3 viewSpaceLightDir;
    float maxDist;
    int numSteps;
  };

  void ScreenSpaceShadowPass::release(SDL_GPUDevice *device) noexcept {
    if (targetTexture)
      SDL_ReleaseGPUTexture(device, targetTexture);

    if (pipeline)
      SDL_ReleaseGPUGraphicsPipeline(device, pipeline);

    if (depthSampler)
      SDL_ReleaseGPUSampler(device, depthSampler);
  }

  void
  ScreenSpaceShadowPass::render(CommandBuffer &cmdBuf, const Camera &camera,
                                const DirectionalLight &light,
                                std::span<const OpaqueCastable> castables) {
    SDL_GPUColorTargetInfo color_target_info;
    SDL_zero(color_target_info);
    color_target_info.texture = targetTexture;
    color_target_info.clear_color = {0., 0., 0., 0.};
    color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;
    color_target_info.cycle = false;

    SDL_GPURenderPass *render_pass =
        SDL_BeginGPURenderPass(cmdBuf, &color_target_info, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

    Mat4f invProj = camera.projection.inverse();
    Mat4f vp = camera.viewProj();
    ScreenSpaceShadowsUniform ubo{
        camera.projection,
        invProj,
        camera.transformVector(light.direction),
        config.maxDist,
        config.numSteps,
    };
    cmdBuf.pushFragmentUniform(0, ubo);
    rend::bindFragmentSamplers(render_pass, 0,
                               {{
                                   .texture = depthTexture,
                                   .sampler = depthSampler,
                               }});

    for (auto &cs : castables) {
      auto &&[mesh, tr] = cs;
      GpuMat4 mvp = vp * tr;
      cmdBuf.pushVertexUniform(0, mvp);
      rend::bindMesh(render_pass, mesh);
      rend::draw(render_pass, mesh);
    }

    SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);

    SDL_EndGPURenderPass(render_pass);
  }

} // namespace effects
} // namespace candlewick
