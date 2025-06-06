#pragma once

#include "Core.h"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <utility>
#include <span>

namespace candlewick {

template <typename T>
concept GpuCompatibleData =
    std::is_standard_layout_v<T> && !std::is_array_v<T> &&
    !std::is_pointer_v<T> &&
    (alignof(T) == 4 || alignof(T) == 8 || alignof(T) == 16);

class CommandBuffer {
  SDL_GPUCommandBuffer *_cmdBuf;

public:
  CommandBuffer(const Device &device);

  /// \brief Convert to SDL_GPU command buffer handle.
  operator SDL_GPUCommandBuffer *() const { return _cmdBuf; }

  /// \brief Deleted copy constructor.
  CommandBuffer(const CommandBuffer &) = delete;

  /// \brief Move constructor.
  CommandBuffer(CommandBuffer &&other) noexcept;

  /// \brief Deleted copy assignment operator.
  CommandBuffer &operator=(const CommandBuffer &) = delete;

  /// \brief Move assignment operator.
  CommandBuffer &operator=(CommandBuffer &&other) noexcept;

  friend void swap(CommandBuffer &lhs, CommandBuffer &rhs) noexcept {
    std::swap(lhs._cmdBuf, rhs._cmdBuf);
  }

  bool submit() noexcept {
    if (!(active() && SDL_SubmitGPUCommandBuffer(_cmdBuf)))
      return false;
    _cmdBuf = nullptr;
    return true;
  }

  SDL_GPUFence *submitAndAcquireFence() noexcept {
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(_cmdBuf);
    _cmdBuf = nullptr;
    return fence;
  }

  /// \brief Cancel the command buffer, returning the bool value from the
  /// wrapped SDL API.
  bool cancel() noexcept;

  /// \brief Check if the command buffer is still active.
  ///
  /// For this wrapper class, it means the internal pointer is non-null.
  bool active() const noexcept { return _cmdBuf; }

  ~CommandBuffer() noexcept {
    if (active()) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "CommandBuffer object is being destroyed while still active! "
                  "It will be cancelled.");
      [[maybe_unused]] bool ret = cancel();
      assert(ret);
    }
  }

  template <GpuCompatibleData T>
  CommandBuffer &pushVertexUniform(Uint32 slot_index, const T &data) {
    return pushVertexUniformRaw(slot_index, &data, sizeof(T));
  }

  template <GpuCompatibleData T>
  CommandBuffer &pushFragmentUniform(Uint32 slot_index, const T &data) {
    return pushFragmentUniformRaw(slot_index, &data, sizeof(T));
  }

  template <GpuCompatibleData T>
  CommandBuffer &pushVertexUniform(Uint32 slot_index, std::span<const T> data) {
    return pushVertexUniformRaw(slot_index, data.data(), data.size_bytes());
  }

  template <GpuCompatibleData T>
  CommandBuffer &pushFragmentUniform(Uint32 slot_index,
                                     std::span<const T> data) {
    return pushFragmentUniformRaw(slot_index, data.data(), data.size_bytes());
  }

  template <GpuCompatibleData T, size_t N>
  CommandBuffer &pushVertexUniform(Uint32 slot_index, const T (&data)[N]) {
    return pushVertexUniform(slot_index, std::span<const T, N>(data));
  }

  template <GpuCompatibleData T, size_t N>
  CommandBuffer &pushFragmentUniform(Uint32 slot_index, const T (&data)[N]) {
    return pushFragmentUniform(slot_index, std::span<const T, N>(data));
  }

  /// \brief Push uniform data to the vertex shader.
  CommandBuffer &pushVertexUniformRaw(Uint32 slot_index, const void *data,
                                      Uint32 length) {
    SDL_PushGPUVertexUniformData(_cmdBuf, slot_index, data, length);
    return *this;
  }
  /// \brief Push uniform data to the fragment shader.
  CommandBuffer &pushFragmentUniformRaw(Uint32 slot_index, const void *data,
                                        Uint32 length) {
    SDL_PushGPUFragmentUniformData(_cmdBuf, slot_index, data, length);
    return *this;
  }
};

} // namespace candlewick
