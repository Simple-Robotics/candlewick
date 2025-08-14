#pragma once

#include "Core.h"
#include "Tags.h"
#include <SDL3/SDL_gpu.h>

namespace candlewick {

/// \brief Automatically detect which subset of shader formats (MSL, SPIR-V) are
/// compatible with the device.
/// \param name Device name. Pass nullptr (default) to auto-detect the best
/// device.
SDL_GPUShaderFormat
auto_detect_shader_format_subset(const char *name = nullptr);

/// \brief RAII wrapper for \c SDL_GPUDevice.
struct Device {

  explicit Device(NoInitT) noexcept;
  explicit Device(SDL_GPUShaderFormat format_flags, bool debug_mode = false);
  Device(const Device &) = delete;
  Device(Device &&other) noexcept {
    _device = other._device;
    other._device = nullptr;
  }

  Device &operator=(const Device &) = delete;
  Device &operator=(Device &&) = delete;

  void create(SDL_GPUShaderFormat format_flags, bool debug_mode = false);

  operator SDL_GPUDevice *() const { return _device; }
  operator bool() const { return _device; }

  const char *driverName() const noexcept;

  SDL_GPUShaderFormat shaderFormats() const {
    return SDL_GetGPUShaderFormats(_device);
  }

  /// \brief Release ownership of and return the \c SDL_GPUDevice handle.
  SDL_GPUDevice *release() noexcept {
    auto *p = _device;
    _device = nullptr;
    return p;
  }
  void destroy() noexcept;

  bool operator==(const Device &other) const {
    return _device && (_device == other._device);
  }

private:
  SDL_GPUDevice *_device;
};

inline Device::Device(NoInitT) noexcept : _device(nullptr) {}

} // namespace candlewick
