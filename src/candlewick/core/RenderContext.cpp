#include "RenderContext.h"
#include "errors.h"
#include <magic_enum/magic_enum.hpp>

#include <utility>
#include <SDL3/SDL_init.h>

namespace candlewick {
RenderContext::RenderContext(Device &&device_, Window &&window_,
                             SDL_GPUTextureFormat suggested_depth_format)
    : device(std::move(device_)), window(std::move(window_)) {
  if (!SDL_ClaimWindowForGPUDevice(device, window))
    throw RAIIException(SDL_GetError());

  createRenderTargets(suggested_depth_format);
}

bool RenderContext::waitAndAcquireSwapchain(CommandBuffer &command_buffer) {
  CANDLEWICK_ASSERT(SDL_IsMainThread(),
                    "Can only acquire swapchain from main thread.");
  return SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window,
                                               &swapchain, NULL, NULL);
}

bool RenderContext::acquireSwapchain(CommandBuffer &command_buffer) {
  CANDLEWICK_ASSERT(SDL_IsMainThread(),
                    "Can only acquire swapchain from main thread.");
  return SDL_AcquireGPUSwapchainTexture(command_buffer, window, &swapchain,
                                        NULL, NULL);
}

void RenderContext::createRenderTargets(
    SDL_GPUTextureFormat suggested_depth_format) {
  auto [width, height] = window.sizeInPixels();

  SDL_GPUTextureCreateInfo colorInfo{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = getSwapchainTextureFormat(),
      .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = Uint32(width),
      .height = Uint32(height),
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
      .props = 0,
  };
  colorBuffer = Texture(device, colorInfo, "Main color target");

  if (suggested_depth_format == SDL_GPU_TEXTUREFORMAT_INVALID)
    return;

  SDL_GPUTextureCreateInfo depthInfo = colorInfo;
  depthInfo.format = suggested_depth_format;
  depthInfo.usage =
      SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTextureFormat depth_format_fallbacks[] = {
      // supported on macOS, supports SAMPLER usage
      SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      // not sure about SAMPLER usage on macOS
      SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
  };

  size_t try_idx = 0;
  while (!SDL_GPUTextureSupportsFormat(device, depthInfo.format, depthInfo.type,
                                       depthInfo.usage) &&
         try_idx < std::size(depth_format_fallbacks)) {
    depthInfo.format = depth_format_fallbacks[try_idx];
    try_idx++;
  }

  depthBuffer = Texture(this->device, depthInfo, "Main depth target");
  spdlog::debug("Created depth texture of format {:s}, size {:d} x {:d}",
                magic_enum::enum_name(depthInfo.format), width, height);
}

void RenderContext::createMsaaTargets(SDL_GPUSampleCount samples) {
  auto [width, height] = window.sizeInPixels();

  SDL_GPUTextureCreateInfo msaaColorInfo{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = colorBuffer.format(),
      .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
      .width = Uint32(width),
      .height = Uint32(height),
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = samples,
      .props = 0,
  };
  if (!SDL_GPUTextureSupportsSampleCount(device, colorBuffer.format(),
                                         samples)) {
    terminate_with_message("Unsupported sample count for MSAA color target.");
  }
  colorMsaa = Texture(device, msaaColorInfo, "MSAA color target");

  if (hasDepthTexture()) {
    // overwrite current depth texture to make it Msaa
    auto depthInfo = depthBuffer.description();
    depthInfo.sample_count = samples;
    depthBuffer = Texture(device, depthInfo, "Main depth target [MSAA]");
  }
}

void RenderContext::presentToSwapchain(CommandBuffer &command_buffer) {
  // NOTE: we always present the resolved color buffer (whether MSAA or not)
  auto [w, h] = window.sizeInPixels();

  SDL_GPUBlitInfo blit{
      .source = colorBuffer.blitRegion(0, 0),
      .destination{
          .texture = swapchain,
          .mip_level = 0,
          .layer_or_depth_plane = 0,
          .x = 0,
          .y = 0,
          .w = Uint32(w),
          .h = Uint32(h),
      },
      .load_op = SDL_GPU_LOADOP_DONT_CARE,
      .clear_color = {},
      .flip_mode = SDL_FLIP_NONE,
      .filter = SDL_GPU_FILTER_LINEAR,
      .cycle = false,
  };
  SDL_BlitGPUTexture(command_buffer, &blit);
}

void RenderContext::destroy() noexcept {
  if (device && window) {
    SDL_ReleaseWindowFromGPUDevice(device, window);
  }
  colorMsaa.destroy();
  colorBuffer.destroy();
  depthBuffer.destroy();
  window.destroy();
  device.destroy();
}

namespace rend {
  void bindMesh(SDL_GPURenderPass *pass, const Mesh &mesh) {
    const Uint32 num_buffers = Uint32(mesh.vertexBuffers.size());
    std::vector<SDL_GPUBufferBinding> vertex_bindings;
    vertex_bindings.reserve(num_buffers);
    for (Uint32 j = 0; j < num_buffers; j++) {
      vertex_bindings.push_back(mesh.getVertexBinding(j));
    }

    SDL_BindGPUVertexBuffers(pass, 0, vertex_bindings.data(), num_buffers);
    if (mesh.isIndexed()) {
      SDL_GPUBufferBinding index_binding = mesh.getIndexBinding();
      SDL_BindGPUIndexBuffer(pass, &index_binding,
                             SDL_GPU_INDEXELEMENTSIZE_32BIT);
    }
  }

  void bindMeshView(SDL_GPURenderPass *pass, const MeshView &meshView) {
    const Uint32 num_buffers = Uint32(meshView.vertexBuffers.size());
    std::vector<SDL_GPUBufferBinding> vertex_bindings;
    vertex_bindings.reserve(num_buffers);
    for (Uint32 j = 0; j < num_buffers; j++) {
      vertex_bindings.push_back({meshView.vertexBuffers[j], 0u});
    }

    SDL_BindGPUVertexBuffers(pass, 0, vertex_bindings.data(), num_buffers);
    if (meshView.isIndexed()) {
      SDL_GPUBufferBinding index_binding = {meshView.indexBuffer, 0u};
      SDL_BindGPUIndexBuffer(pass, &index_binding,
                             SDL_GPU_INDEXELEMENTSIZE_32BIT);
    }
  }

  void drawView(SDL_GPURenderPass *pass, const MeshView &mesh,
                Uint32 numInstances) {
    assert(validateMeshView(mesh));
    if (mesh.isIndexed()) {
      SDL_DrawGPUIndexedPrimitives(pass, mesh.indexCount, numInstances,
                                   mesh.indexOffset, Sint32(mesh.vertexOffset),
                                   0);
    } else {
      SDL_DrawGPUPrimitives(pass, mesh.vertexCount, numInstances,
                            mesh.vertexOffset, 0);
    }
  }

  void drawViews(SDL_GPURenderPass *pass, std::span<const MeshView> meshViews,
                 Uint32 numInstances) {
    if (meshViews.empty())
      return;

#ifndef NDEBUG
    const auto ib = meshViews[0].indexBuffer;
    const auto &vbs = meshViews[0].vertexBuffers;
    const auto n_vbs = vbs.size();
#endif
    for (auto &view : meshViews) {
#ifndef NDEBUG
      CANDLEWICK_ASSERT(ib == view.indexBuffer,
                        "Invalid view set (different index buffers)");
      for (size_t i = 0; i < n_vbs; i++)
        CANDLEWICK_ASSERT(vbs[i] == view.vertexBuffers[i],
                          "Invalid view set (different vertex buffers)");
#endif
      drawView(pass, view, numInstances);
    }
  }

} // namespace rend

} // namespace candlewick
