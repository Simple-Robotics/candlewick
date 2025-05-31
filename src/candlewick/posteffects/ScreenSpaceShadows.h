#pragma once

#include "../core/Core.h"
#include "../core/Tags.h"
#include "../core/LightUniforms.h"

#include <SDL3/SDL_gpu.h>
#include <span>

namespace candlewick {
namespace effects {

  /// \brief WIP screen space shadows
  struct ScreenSpaceShadowPass {
    struct Config {
      float maxDist = 1.0f;
      int numSteps = 16;
    } config;
    SDL_GPUTexture *depthTexture = nullptr;
    /// Sampler for the depth texture (e.g. from the prepass)
    SDL_GPUSampler *depthSampler = nullptr;
    /// Target texture to render the shadow to.
    SDL_GPUTexture *targetTexture = nullptr;
    /// Render pipeline
    SDL_GPUGraphicsPipeline *pipeline = nullptr;

    bool valid() const {
      return depthTexture && depthSampler && targetTexture && pipeline;
    }

    ScreenSpaceShadowPass(NoInitT) {}
    ScreenSpaceShadowPass(const RenderContext &renderer, const Config &config);

    void release(SDL_GPUDevice *device) noexcept;

    void render(CommandBuffer &cmdBuf, const Camera &camera,
                const DirectionalLight &light,
                std::span<const OpaqueCastable> castables);
  };

} // namespace effects
} // namespace candlewick
