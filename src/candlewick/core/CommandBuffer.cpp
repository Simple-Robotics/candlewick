#include "CommandBuffer.h"
#include "Device.h"

namespace candlewick {

CommandBuffer::CommandBuffer(const Device &device) {
  _cmdBuf = SDL_AcquireGPUCommandBuffer(device);
}

CommandBuffer::CommandBuffer(CommandBuffer &&other) noexcept
    : _cmdBuf(other._cmdBuf) {
  other._cmdBuf = nullptr;
}

CommandBuffer &CommandBuffer::operator=(CommandBuffer &&other) noexcept {
  if (active()) {
    this->cancel();
  }
  _cmdBuf = other._cmdBuf;
  other._cmdBuf = nullptr;
  return *this;
}

bool CommandBuffer::cancel() noexcept {
  bool ret = SDL_CancelGPUCommandBuffer(_cmdBuf);
  _cmdBuf = nullptr;
  if (!ret) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Failed to cancel command buffer: %s", SDL_GetError());
    return false;
  }
  return true;
}

} // namespace candlewick
