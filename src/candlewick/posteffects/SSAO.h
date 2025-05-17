#pragma once

#include "../core/Core.h"
#include "../core/Texture.h"
#include <SDL3/SDL_gpu.h>

namespace candlewick {
namespace ssao {
  struct SsaoPass {
    SDL_GPUDevice *_device = nullptr;
    SDL_GPUTexture *inDepthMap = nullptr;
    SDL_GPUTexture *inNormalMap = nullptr;
    SDL_GPUSampler *texSampler = nullptr;
    SDL_GPUGraphicsPipeline *pipeline = nullptr;
    Texture ssaoMap{NoInit};
    struct SsaoNoise {
      Texture tex{NoInit};
      SDL_GPUSampler *sampler = nullptr;
      // The texture will be N x N where N is this value.
      Uint32 pixel_window_size;
    } ssaoNoise;
    SDL_GPUGraphicsPipeline *blurPipeline = nullptr;
    // first blur pass target
    Texture blurPass1Tex{NoInit};

    SsaoPass(NoInitT) {}
    SsaoPass(const Renderer &renderer, SDL_GPUTexture *normalMap);

    SsaoPass(SsaoPass &&other) noexcept;
    SsaoPass &operator=(SsaoPass &&other) noexcept;

    void render(CommandBuffer &cmdBuf, const Camera &camera);

    // cleanup function
    void release() noexcept;

    ~SsaoPass() noexcept { this->release(); }
  };

} // namespace ssao
} // namespace candlewick
