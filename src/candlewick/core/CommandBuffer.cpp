#include "CommandBuffer.h"
#include "Device.h"

namespace candlewick {

CommandBuffer::CommandBuffer(const Device &device) {
  m_handle = SDL_AcquireGPUCommandBuffer(device);
}

bool CommandBuffer::cancel() noexcept {
  bool ret = SDL_CancelGPUCommandBuffer(m_handle);
  m_handle = nullptr;
  if (!ret) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Failed to cancel command buffer: %s", SDL_GetError());
    return false;
  }
  return true;
}

} // namespace candlewick
