#include "Device.h"
#include "errors.h"

#include <spdlog/spdlog.h>

namespace candlewick {

SDL_GPUShaderFormat auto_detect_shader_format_subset(const char *name) {
  SDL_GPUShaderFormat available_formats = SDL_GPU_SHADERFORMAT_INVALID;
  SDL_GPUShaderFormat formats[]{SDL_GPU_SHADERFORMAT_SPIRV,
                                SDL_GPU_SHADERFORMAT_DXIL,
                                SDL_GPU_SHADERFORMAT_MSL};
  for (SDL_GPUShaderFormat test_format : formats) {
    if (SDL_GPUSupportsShaderFormats(test_format, name))
      available_formats |= test_format;
  }
  return available_formats;
}

Device::Device(SDL_GPUShaderFormat format_flags, bool debug_mode) {
  create(format_flags, debug_mode);
}

void Device::create(SDL_GPUShaderFormat format_flags, bool debug_mode) {
  _device = SDL_CreateGPUDevice(format_flags, debug_mode, nullptr);
  if (!_device)
    throw RAIIException(SDL_GetError());
  spdlog::info("Device driver: {:s}", driverName());
}

const char *Device::driverName() const noexcept {
  return SDL_GetGPUDeviceDriver(_device);
}

void Device::destroy() noexcept {
  if (_device) {
    spdlog::debug("Destroying GPU device");
    SDL_DestroyGPUDevice(_device);
  }
  _device = nullptr;
}

} // namespace candlewick
