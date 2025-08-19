/// \copyright Copyright (c) 2025 Inria
#pragma once

#include "Device.h"
#include "CommandBuffer.h"
#include "Texture.h"
#include "Mesh.h"
#include "Window.h"

#include <span>
#include <SDL3/SDL_gpu.h>

namespace candlewick {

inline constexpr int sdlSampleToValue(SDL_GPUSampleCount samples) {
  switch (samples) {
  case SDL_GPU_SAMPLECOUNT_1:
    return 1;
  case SDL_GPU_SAMPLECOUNT_2:
    return 2;
  case SDL_GPU_SAMPLECOUNT_4:
    return 4;
  case SDL_GPU_SAMPLECOUNT_8:
    return 8;
  }
}

/// \brief The RenderContext class provides a rendering context for a graphical
/// application.
///
/// \sa Scene
/// \sa Device
/// \sa Mesh
struct RenderContext {
private:
  Texture colorMsaa{NoInit};
  Texture colorBuffer{NoInit}; // no MSAA
  Texture depthBuffer{NoInit}; // no MSAA
  bool m_msaaEnabled = false;
  SDL_GPUTexture *swapchain{nullptr};

  void createMsaaTargets(SDL_GPUSampleCount samples);
  void createRenderTargets(SDL_GPUTextureFormat suggested_depth_format);

public:
  Device device;
  Window window;

  RenderContext(NoInitT) : device(NoInit), window(nullptr) {}
  /// \brief Constructor.
  /// The last argument (the depth texture format) is optional. By default, it
  /// is set to INVALID, and no depth texture is created.
  RenderContext(Device &&device, Window &&window,
                SDL_GPUTextureFormat suggested_depth_format =
                    SDL_GPU_TEXTUREFORMAT_INVALID);

  RenderContext(RenderContext &&) noexcept = default;
  RenderContext &operator=(RenderContext &&) noexcept = default;

  const Texture &colorTarget() const {
    return (m_msaaEnabled && colorMsaa) ? colorMsaa : colorBuffer;
  }

  const Texture &depthTarget() const { return depthBuffer; }

  const Texture &resolvedColorTarget() const { return colorBuffer; }

  bool msaaEnabled() const { return m_msaaEnabled; }

  SDL_GPUSampleCount getMsaaSampleCount() const {
    if (m_msaaEnabled && colorMsaa) {
      return colorMsaa.sampleCount();
    }
    return SDL_GPU_SAMPLECOUNT_1;
  }

  void enableMSAA(SDL_GPUSampleCount samples) {
    if (samples > SDL_GPU_SAMPLECOUNT_1) {
      m_msaaEnabled = true;
      createMsaaTargets(samples);
      int sample_size = sdlSampleToValue(samples);
      SDL_Log("MSAA enabled with %d samples", sample_size);
    } else {
      disableMSAA();
    }
  }

  void disableMSAA() {
    m_msaaEnabled = false;
    colorMsaa.destroy();
    SDL_Log("MSAA disabled.");
  }

  bool initialized() const { return bool(device); }

  /// Acquire the command buffer, starting a frame.
  CommandBuffer acquireCommandBuffer() const { return CommandBuffer(device); }

  /// \brief Wait until swapchain is available, then acquire it.
  /// \sa acquireSwapchain()
  bool waitAndAcquireSwapchain(CommandBuffer &command_buffer);

  /// \brief Acquire GPU swapchain.
  /// \warning This can only be called from the main thread (see SDL docs for
  /// the meaning of "main thread").
  bool acquireSwapchain(CommandBuffer &command_buffer);

  /// \brief Wait for the swapchain to be available.
  bool waitForSwapchain() { return SDL_WaitForGPUSwapchain(device, window); }

  /// \brief Present the resolved texture to the swapchain. You must acquire the
  /// swapchain beforehand.
  void presentToSwapchain(CommandBuffer &command_buffer);

  SDL_GPUTextureFormat getSwapchainTextureFormat() const {
    return SDL_GetGPUSwapchainTextureFormat(device, window);
  }

  /// \brief Check if a depth texture was created.
  inline bool hasDepthTexture() const { return depthBuffer; }

  inline void setSwapchainParameters(SDL_GPUSwapchainComposition composition,
                                     SDL_GPUPresentMode present_mode) {
    SDL_SetGPUSwapchainParameters(device, window, composition, present_mode);
  }

  SDL_GPUTextureFormat colorFormat() const { return colorBuffer.format(); }
  SDL_GPUTextureFormat depthFormat() const { return depthBuffer.format(); }

  void destroy() noexcept;
  ~RenderContext() noexcept { this->destroy(); }
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
