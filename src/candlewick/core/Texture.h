#pragma once

#include "Core.h"
#include "Tags.h"
#include <SDL3/SDL_gpu.h>

namespace candlewick {

class Texture {
  SDL_GPUDevice *m_device = nullptr;
  SDL_GPUTexture *m_texture = nullptr;
  SDL_GPUTextureCreateInfo m_description;

public:
  Texture(NoInitT) {}
  Texture(const Device &device, SDL_GPUTextureCreateInfo texture_desc,
          const char *name = nullptr);

  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;
  Texture(Texture &&other) noexcept;
  Texture &operator=(Texture &&other) noexcept;

  operator SDL_GPUTexture *() const noexcept { return m_texture; }

  bool operator==(const Texture &other) const noexcept {
    return m_texture == other.m_texture;
  }

  const auto &description() const { return m_description; }
  SDL_GPUTextureType type() const { return m_description.type; }
  SDL_GPUTextureFormat format() const { return m_description.format; }
  SDL_GPUTextureUsageFlags usage() const { return m_description.usage; }
  Uint32 width() const { return m_description.width; }
  Uint32 height() const { return m_description.height; }
  Uint32 depth() const { return m_description.layer_count_or_depth; }
  Uint32 layerCount() const { return m_description.layer_count_or_depth; }
  SDL_GPUSampleCount sampleCount() const { return m_description.sample_count; }
  std::string_view name() const {
    return SDL_GetStringProperty(
        m_description.props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, "(null)");
  }

  SDL_GPUBlitRegion blitRegion(Uint32 offset_x, Uint32 y_offset,
                               Uint32 layer_or_depth_plane = 0) const;

  Uint32 textureSize() const;

  SDL_GPUDevice *device() const { return m_device; }

  void destroy() noexcept;
  ~Texture() noexcept { this->destroy(); }
};

} // namespace candlewick
