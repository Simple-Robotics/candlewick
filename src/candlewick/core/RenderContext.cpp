#include "Renderer.h"
#include "errors.h"
#include <magic_enum/magic_enum.hpp>

#include <utility>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>

namespace candlewick {
RenderContext::RenderContext(Device &&device_, Window &&window_)
    : device(std::move(device_))
    , window(std::move(window_))
    , swapchain(nullptr) {
  if (!SDL_ClaimWindowForGPUDevice(device, window))
    throw RAIIException(SDL_GetError());
}

RenderContext::RenderContext(Device &&device_, Window &&window_,
                             SDL_GPUTextureFormat suggested_depth_format)
    : RenderContext(std::move(device_), std::move(window_)) {
  createDepthTexture(suggested_depth_format);
}

void RenderContext::createDepthTexture(
    SDL_GPUTextureFormat suggested_depth_format) {
  auto [width, height] = window.size();

  SDL_GPUTextureCreateInfo texInfo{
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = suggested_depth_format,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET |
               SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = Uint32(width),
      .height = Uint32(height),
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .sample_count = SDL_GPU_SAMPLECOUNT_1,
      .props = 0,
  };
  SDL_GPUTextureFormat depth_format_fallbacks[] = {
      // supported on macOS, supports SAMPLER usage
      SDL_GPU_TEXTUREFORMAT_D16_UNORM,
      // not sure about SAMPLER usage on macOS
      SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
  };
  size_t try_idx = 0;
  while (!SDL_GPUTextureSupportsFormat(device, texInfo.format, texInfo.type,
                                       texInfo.usage) &&
         try_idx < std::size(depth_format_fallbacks)) {
    texInfo.format = depth_format_fallbacks[try_idx];
    try_idx++;
  }
  depth_texture = Texture(this->device, texInfo);
  SDL_Log("Created depth texture of format %s, size %d x %d\n",
          magic_enum::enum_name(texInfo.format).data(), width, height);
  SDL_SetGPUTextureName(device, depth_texture, "Main depth texture");
}

bool RenderContext::waitAndAcquireSwapchain(CommandBuffer &command_buffer) {
  assert(SDL_IsMainThread());
  return SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window,
                                               &swapchain, NULL, NULL);
}

bool RenderContext::acquireSwapchain(CommandBuffer &command_buffer) {
  assert(SDL_IsMainThread());
  return SDL_AcquireGPUSwapchainTexture(command_buffer, window, &swapchain,
                                        NULL, NULL);
}

RenderContext::~RenderContext() noexcept {
  if (device && window) {
    SDL_ReleaseWindowFromGPUDevice(device, window);
  }
  depth_texture.destroy();
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
      SDL_assert(ib == view.indexBuffer);
      for (size_t i = 0; i < n_vbs; i++)
        SDL_assert(vbs[i] == view.vertexBuffers[i]);
#endif
      drawView(pass, view, numInstances);
    }
  }

} // namespace rend

} // namespace candlewick
