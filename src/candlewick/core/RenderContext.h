#pragma once

#include "Device.h"
#include "CommandBuffer.h"
#include "Texture.h"
#include "Mesh.h"
#include "Window.h"

#include <span>
#include <SDL3/SDL_gpu.h>

namespace candlewick {

/// \brief The RenderContext class provides a rendering context for a graphical
/// application.
///
/// \sa Scene
/// \sa Device
/// \sa Mesh
struct RenderContext {
  Device device;
  Window window;
  SDL_GPUTexture *swapchain;
  Texture depth_texture{NoInit};

  RenderContext(NoInitT)
      : device(NoInit), window(nullptr), swapchain(nullptr) {}
  /// \brief Constructor without a depth format.
  RenderContext(Device &&device, Window &&window);
  /// \brief Constructor with a depth format. This will create a depth texture.
  RenderContext(Device &&device, Window &&window,
                SDL_GPUTextureFormat suggested_depth_format);

  /// \brief Add a depth texture to the rendering context.
  /// \see hasDepthTexture()
  void createDepthTexture(SDL_GPUTextureFormat suggested_depth_format);

  bool initialized() const { return bool(device); }

  /// Acquire the command buffer, starting a frame.
  CommandBuffer acquireCommandBuffer() const { return CommandBuffer(device); }

  /// \brief Wait until swapchain is available, then acquire it.
  /// \sa acquireSwapchain()
  bool waitAndAcquireSwapchain(CommandBuffer &command_buffer) {
    assert(SDL_IsMainThread());
    return SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window,
                                                 &swapchain, NULL, NULL);
  }

  /// \brief Acquire GPU swapchain.
  /// \warning This can only be called from the main thread (see SDL docs for
  /// the meaning of "main thread").
  bool acquireSwapchain(CommandBuffer &command_buffer) {
    assert(SDL_IsMainThread());
    return SDL_AcquireGPUSwapchainTexture(command_buffer, window, &swapchain,
                                          NULL, NULL);
  }

  bool waitForSwapchain() { return SDL_WaitForGPUSwapchain(device, window); }

  SDL_GPUTextureFormat getSwapchainTextureFormat() const {
    return SDL_GetGPUSwapchainTextureFormat(device, window);
  }

  /// \brief Check if a depth texture was created.
  bool hasDepthTexture() const { return depth_texture.hasValue(); }

  SDL_GPUTextureFormat depthFormat() const { return depth_texture.format(); }

  ~RenderContext() noexcept;

  void destroy() noexcept { this->~RenderContext(); }
};

namespace rend {

  /// \brief Bind a Mesh object.
  /// \sa bindMeshView()
  void bindMesh(SDL_GPURenderPass *pass, const Mesh &mesh);

  /// \brief Bind a MeshView object.
  /// \warning This performs the same logic as bindMesh(). The entire buffer is
  /// bound, because drawView() uses the vertex and index offsets to draw.
  /// Binding offset buffers would, then, be incorrect.
  /// \sa bindMesh()
  /// \sa drawView()
  /// \sa drawViews()
  void bindMeshView(SDL_GPURenderPass *pass, const MeshView &view);

  /// \brief Render a collection of MeshView.
  /// This collection must satisfy the invariant that they all have the same
  /// parent Mesh object, which allows batching draw calls like this without
  /// having to rebind the Mesh.
  /// \param pass The GPU render pass
  /// \param meshViews Collection of MeshView objects with the same parent Mesh.
  /// \sa drawView() (for individual views)
  void drawViews(SDL_GPURenderPass *pass, std::span<const MeshView> meshViews,
                 Uint32 numInstances = 1);

  /// Render an individual Mesh as part of a render pass, using a provided first
  /// index or vertex offset.
  /// \details This routine will bind the vertex and index buffer. Prefer one of
  /// the other routines to e.g. batch draw calls that use the same buffer
  /// bindings.
  /// \warning Call bindMesh() first!
  /// \param pass The GPU render pass
  /// \param mesh The Mesh to draw.
  inline void draw(SDL_GPURenderPass *pass, const Mesh &mesh,
                   Uint32 numInstances = 1) {
    drawViews(pass, mesh.views(), numInstances);
  }

  /// \brief Draw a MeshView.
  /// \warning Call bindMesh() first!
  /// \param pass The GPU render pass.
  /// \param mesh MeshView object to be drawn.
  /// \sa draw()
  void drawView(SDL_GPURenderPass *pass, const MeshView &mesh,
                Uint32 numInstances = 1);

  /// \brief Bind multiple fragment shader samplers.
  inline void
  bindVertexSamplers(SDL_GPURenderPass *pass, Uint32 first_slot,
                     std::span<const SDL_GPUTextureSamplerBinding> bindings) {
    SDL_BindGPUVertexSamplers(pass, first_slot, bindings.data(),
                              Uint32(bindings.size()));
  }

  /// \copybrief bindFragmentSamplers()
  /// This overload exists to enable taking a brace-initialized array.
  inline void bindVertexSamplers(
      SDL_GPURenderPass *pass, Uint32 first_slot,
      std::initializer_list<SDL_GPUTextureSamplerBinding> sampler_bindings) {
    bindVertexSamplers(pass, first_slot, std::span(sampler_bindings));
  }

  /// \brief Bind multiple fragment shader samplers.
  inline void
  bindFragmentSamplers(SDL_GPURenderPass *pass, Uint32 first_slot,
                       std::span<const SDL_GPUTextureSamplerBinding> bindings) {
    SDL_BindGPUFragmentSamplers(pass, first_slot, bindings.data(),
                                Uint32(bindings.size()));
  }

  /// \copybrief bindFragmentSamplers()
  /// This overload exists to enable taking a brace-initialized array.
  inline void bindFragmentSamplers(
      SDL_GPURenderPass *pass, Uint32 first_slot,
      std::initializer_list<SDL_GPUTextureSamplerBinding> sampler_bindings) {
    bindFragmentSamplers(pass, first_slot, std::span(sampler_bindings));
  }

} // namespace rend
} // namespace candlewick
