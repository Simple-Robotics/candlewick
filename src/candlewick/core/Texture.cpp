#include "Texture.h"
#include "Device.h"
#include "errors.h"
#include <magic_enum/magic_enum.hpp>

#include <format>

namespace candlewick {

Texture::Texture(const Device &device, SDL_GPUTextureCreateInfo texture_desc,
                 const char *name)
    : _texture(nullptr), _device(&device),
      _description(std::move(texture_desc)) {
  if (!(_texture = SDL_CreateGPUTexture(*_device, &_description))) {
    std::string msg = std::format("Failed to create texture with format (%s)",
                                  magic_enum::enum_name(_description.format));
    if (name)
      msg += std::format(" (name %s)", name);
    throw RAIIException(std::move(msg));
  }
  if (name != nullptr)
    SDL_SetGPUTextureName(*_device, _texture, name);
}

Texture::Texture(Texture &&other) noexcept {
  _device = other._device;
  _texture = other._texture;
  _description = std::move(other._description);

  other._device = nullptr;
  other._texture = nullptr;
}

Texture &Texture::operator=(Texture &&other) noexcept {
  this->destroy();
  _device = other._device;
  _texture = other._texture;
  _description = std::move(other._description);

  other._device = nullptr;
  other._texture = nullptr;
  return *this;
}

SDL_GPUBlitRegion Texture::blitRegion(Uint32 x, Uint32 y,
                                      Uint32 layer_or_depth_plane) const {
  CDW_ASSERT(layer_or_depth_plane < layerCount(),
             "layer is higher than layerCount!");
  return {
      .texture = _texture,
      .mip_level = 0,
      .layer_or_depth_plane = layer_or_depth_plane,
      .x = x,
      .y = y,
      .w = width(),
      .h = height(),
  };
}

Uint32 Texture::textureSize() const {
  return SDL_CalculateGPUTextureFormatSize(format(), width(), height(),
                                           depth());
}

const Device &Texture::device() const { return *_device; }

void Texture::destroy() noexcept {
  if (_device && _texture) {
    SDL_ReleaseGPUTexture(*_device, _texture);
    _texture = nullptr;
    _device = nullptr;
  }
}
} // namespace candlewick
