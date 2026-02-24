#include "SSAO.h"

#include "../core/CommandBuffer.h"
#include "../core/Shader.h"
#include "../core/Camera.h"
#include "../core/RenderContext.h"
#include "../third-party/float16_t.hpp"
#include <random>

namespace candlewick {
namespace ssao {
  using numeric::float16_t;
  using Float16x2 = Eigen::Matrix<float16_t, 2, 1, Eigen::DontAlign>;

  float lerp(float a, float b, float f) { return a + f * (b - a); }

  // https://sudonull.com/post/102169-Normal-oriented-Hemisphere-SSAO-for-Dummies
  std::vector<GpuVec4> generateSsaoKernel(size_t kernelSize = 64ul) {
    std::random_device rd;
    Uint32 seed = rd();
    std::mt19937 gen{seed};
    std::uniform_real_distribution<float> rfloat{0., 1.};

    std::vector<GpuVec4> kernel;
    kernel.resize(kernelSize);

    for (size_t i = 0; i < kernelSize; i++) {
      GpuVec3 sample;
      sample.setRandom();
      sample.head<2>() *= 2.f;
      sample.head<2>().array() -= 1.f;
      sample.normalize();
      sample *= rfloat(gen);

      float scale = float(i) / float(kernelSize);
      scale = lerp(0.1f, 10.f, scale * scale);
      sample *= scale;

      kernel[i].head<3>() = sample;
      kernel[i].w() = 1.f;
    }
    return kernel;
  }

  static const std::vector KERNEL_SAMPLES = generateSsaoKernel();

  Texture create_noise_texture(const Device &device, Uint32 size) {
    return Texture{device,
                   {.type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT,
                    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                    .width = size,
                    .height = size,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                    .sample_count = SDL_GPU_SAMPLECOUNT_1,
                    .props = 0}};
  }

  auto generateNoiseTextureValues(Uint32 tex_size) {
    static_assert(sizeof(Float16x2) == 2 * 2);
    Uint32 noise_size = tex_size * tex_size;

    std::vector<Float16x2> noise;
    noise.resize(noise_size);
    std::uniform_real_distribution<float> rfloat{0., 1.};
    std::random_device rd;
    std::mt19937 gen{rd()};

    for (size_t i = 0; i < noise_size; i++) {
      GpuVec2 sample{2.f * rfloat(gen) - 1.f, 2.f * rfloat(gen) - 1.f};
      sample.normalize();
      noise[i] = sample.cast<float16_t>();
    }
    return noise;
  }

  SsaoPass::SsaoNoise createSsaoNoise(const Device &dev, Uint32 size) {
    Texture tex = create_noise_texture(dev, size);
    SDL_GPUSamplerCreateInfo sampler_ci{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    };
    return {std::move(tex), SDL_CreateGPUSampler(dev, &sampler_ci), size};
  }

  static bool pushSsaoNoiseData(const Device &dev,
                                const SsaoPass::SsaoNoise &noise) {
    auto values = generateNoiseTextureValues(noise.pixel_window_size);
    using element_type = decltype(values)::value_type;
    const Texture &tex = noise.tex;
    CommandBuffer command_buffer(dev);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    auto payload_size = Uint32(values.size() * sizeof(values[0]));
    assert(payload_size == tex.textureSize());
    SDL_GPUTransferBufferCreateInfo tb_ci{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = payload_size,
        .props = 0};
    SDL_GPUTransferBuffer *texTransferBuffer =
        SDL_CreateGPUTransferBuffer(dev, &tb_ci);
    auto *data =
        (element_type *)SDL_MapGPUTransferBuffer(dev, texTransferBuffer, false);
    std::copy(values.begin(), values.end(), data);
    SDL_UnmapGPUTransferBuffer(dev, texTransferBuffer);

    SDL_GPUTextureTransferInfo tex_trans_info{
        .transfer_buffer = texTransferBuffer, .offset = 0};
    SDL_GPUTextureRegion tex_region{
        .texture = tex, .w = tex.width(), .h = tex.height(), .d = 1};
    assert(tex_region.d == tex.depth());
    SDL_UploadToGPUTexture(copy_pass, &tex_trans_info, &tex_region, false);

    SDL_EndGPUCopyPass(copy_pass);
    return command_buffer.submit();
  }

  SsaoPass::SsaoPass(SsaoPass &&other) noexcept
      : _device(other._device)
      , inDepthMap(other.inDepthMap)
      , inNormalMap(other.inNormalMap)
      , texSampler(other.texSampler)
      , pipeline(std::move(other.pipeline))
      , ssaoMap(std::move(other.ssaoMap))
      , ssaoNoise(std::move(other.ssaoNoise))
      , blurPipeline(std::move(other.blurPipeline))
      , blurPass1Tex(std::move(other.blurPass1Tex)) {
    other._device = nullptr;
  }

  SsaoPass &SsaoPass::operator=(SsaoPass &&other) noexcept {
    if (this != &other) {
      this->release();
#define _c(name) name = std::move(other.name)
      _c(_device);
      _c(inDepthMap);
      _c(inNormalMap);
      _c(texSampler);
      _c(pipeline);
      _c(ssaoMap);
      _c(ssaoNoise);
      _c(blurPipeline);
      _c(blurPass1Tex);
#undef _c

      other._device = nullptr;
    }
    return *this;
  }

  SsaoPass::SsaoPass(const RenderContext &renderer, SDL_GPUTexture *normalMap,
                     SDL_GPUTexture *depthTex)
      : _device(renderer.device), inDepthMap(depthTex), inNormalMap(normalMap) {
    const auto &device = renderer.device;

    SDL_GPUSamplerCreateInfo samplers_ci{
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    texSampler = SDL_CreateGPUSampler(device, &samplers_ci);

    auto [width, height] = renderer.window.size();
    SDL_GPUTextureCreateInfo texture_desc{
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
        .usage =
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = Uint32(width),
        .height = Uint32(height),
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0,
    };
    ssaoMap = Texture{device, texture_desc, "SSAO map (pre-blur)"};
    blurPass1Tex = Texture{device, texture_desc, "SSAO output map"};

    auto vertexShader = Shader::fromMetadata(device, "DrawQuad.vert");
    auto fragmentShader = Shader::fromMetadata(device, "SSAO.frag");
    SDL_GPUColorTargetDescription color_desc;
    SDL_zero(color_desc);
    // render AO map to 16-bit float texture
    color_desc.format = texture_desc.format;
    SDL_GPUGraphicsPipelineCreateInfo pipeline_desc{
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state{.fill_mode = SDL_GPU_FILLMODE_FILL,
                          .cull_mode = SDL_GPU_CULLMODE_BACK},
        .target_info{.color_target_descriptions = &color_desc,
                     .num_color_targets = 1,
                     .has_depth_stencil_target = false},
    };
    pipeline = GraphicsPipeline(device, pipeline_desc, "SSAO pipeline");
    auto blurShader = Shader::fromMetadata(device, "SSAOblur.frag");
    SDL_GPUGraphicsPipelineCreateInfo blur_pipeline_desc = pipeline_desc;
    blur_pipeline_desc.fragment_shader = blurShader;
    blurPipeline =
        GraphicsPipeline(device, blur_pipeline_desc, "SSAO pipeline [blur]");

    // Now, we create the noise texture
    Uint32 num_pixels_rows = 4u;
    ssaoNoise = createSsaoNoise(device, num_pixels_rows);
    assert(num_pixels_rows == ssaoNoise.pixel_window_size);
    pushSsaoNoiseData(device, ssaoNoise);
  }

  void SsaoPass::render(CommandBuffer &cmdBuf, const Camera &camera) {
    GpuMat4 proj = camera.projection;
    SDL_GPUColorTargetInfo color_info{
        .texture = ssaoMap,
        .layer_or_depth_plane = 0,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *render_pass =
        SDL_BeginGPURenderPass(cmdBuf, &color_info, 1, nullptr);

    rend::bindFragmentSamplers(
        render_pass, 0,
        {
            {.texture = inDepthMap, .sampler = texSampler},
            {.texture = inNormalMap, .sampler = texSampler},
            {.texture = ssaoNoise.tex, .sampler = ssaoNoise.sampler},
        });
    auto SAMPLES_PAYLOAD_BYTES =
        Uint32(KERNEL_SAMPLES.size() * sizeof(GpuVec4));
    cmdBuf
        .pushFragmentUniformRaw(0, KERNEL_SAMPLES.data(), SAMPLES_PAYLOAD_BYTES)
        .pushFragmentUniform(1, proj);
    pipeline.bind(render_pass);
    SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(render_pass);

    const GpuVec2 blurDirections[] = {{1, 0}, {0, 1}};
    for (size_t i = 0; i < 2; i++) {
      const GpuVec2 blurDir = blurDirections[i];
      // if i = 0, render to pass 1 blur texture
      color_info.texture = (i == 0) ? blurPass1Tex : ssaoMap;

      render_pass = SDL_BeginGPURenderPass(cmdBuf, &color_info, 1, nullptr);
      blurPipeline.bind(render_pass);

      cmdBuf.pushFragmentUniform(0, blurDir);
      rend::bindFragmentSamplers(
          render_pass, 0,
          {{
              .texture = (i == 0) ? ssaoMap : blurPass1Tex,
              .sampler = texSampler,
          }});
      SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);
      SDL_EndGPURenderPass(render_pass);
    }
  }

  void SsaoPass::release() noexcept {
    if (_device) {
      if (texSampler)
        SDL_ReleaseGPUSampler(_device, texSampler);
      texSampler = nullptr;
      if (ssaoNoise.sampler)
        SDL_ReleaseGPUSampler(_device, ssaoNoise.sampler);
      ssaoNoise.sampler = nullptr;
      _device = nullptr;
    }
    pipeline.release();
    blurPipeline.release();
    ssaoMap.destroy();
    ssaoNoise.tex.destroy();
    blurPass1Tex.destroy();
  }

} // namespace ssao
} // namespace candlewick
