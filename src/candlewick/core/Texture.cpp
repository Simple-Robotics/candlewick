#include "Texture.h"
#include "Device.h"
#include "errors.h"
#include <magic_enum/magic_enum.hpp>

#include <format>

namespace candlewick {

Texture::Texture(const Device &device, SDL_GPUTextureCreateInfo texture_desc,
                 const char *name)
    : m_device(device)
    , m_texture(nullptr)
    , m_description(std::move(texture_desc)) {
  if (!m_description.props)
    m_description.props = SDL_CreateProperties();
  if (name != nullptr) {
    SDL_SetStringProperty(m_description.props,
                          SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, name);
  }
  if (!(m_texture = SDL_CreateGPUTexture(m_device, &m_description))) {
    std::string msg = std::format("Failed to create texture with format (%s)",
                                  magic_enum::enum_name(m_description.format));
    if (name)
      msg += std::format(" (name {:s})", name);
    throw RAIIException(std::move(msg));
  }
}

Texture::Texture(Texture &&other) noexcept
    : m_device(other.m_device)
    , m_texture(other.m_texture)
    , m_description(std::move(other.m_description)) {
  other.m_device = nullptr;
  other.m_texture = nullptr;
}

Texture &Texture::operator=(Texture &&other) noexcept {
  if (this != &other) {
    this->destroy();
    m_device = other.m_device;
    m_texture = other.m_texture;
    m_description = std::move(other.m_description);

    other.m_device = nullptr;
    other.m_texture = nullptr;
  }
  return *this;
}

SDL_GPUBlitRegion Texture::blitRegion(Uint32 x, Uint32 y,
                                      Uint32 layer_or_depth_plane) const {
  CANDLEWICK_ASSERT(layer_or_depth_plane < layerCount(),
                    "layer is higher than layerCount!");
  return {
      .texture = m_texture,
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

void Texture::destroy() noexcept {
  if (m_device && m_texture) {
    SDL_ReleaseGPUTexture(m_device, m_texture);
    m_texture = nullptr;
    m_device = nullptr;
  }
}
} // namespace candlewick
