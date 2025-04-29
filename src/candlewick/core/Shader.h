/// \defgroup shaders Shaders
#pragma once

#include "Core.h"
#include <SDL3/SDL_gpu.h>

namespace candlewick {

constexpr const char *g_default_shader_dir = CANDLEWICK_SHADER_BIN_DIR;

/// \brief Set the current (global) directory where shaders are to be found.
/// \sa currentShaderDirectory()
void setShadersDirectory(const char *path);
/// \brief Get the current (global) directory where shaders are found.
const char *currentShaderDirectory();

SDL_GPUShaderStage detect_shader_stage(const char *filename);

const char *shader_format_name(SDL_GPUShaderFormat shader_format);

/// \ingroup shaders
/// \brief RAII wrapper around \c SDL_GPUShader, with loading utilities.
///
/// The Shader class wrap around the SDL GPU shader handle. It creates it upon
/// construction of the class, and will release it upon destruction.
struct Shader {
  /// \brief %Shader configuration: number of uniforms, texture samplers,
  /// storage textures and storage buffers.
  struct Config {
    Uint32 uniform_buffers;
    Uint32 samplers;
    Uint32 storage_textures = 0;
    Uint32 storage_buffers = 0;
  };
  /// \brief Load a shader to device provided the shader name and configuration.
  /// \param device GPU device
  /// \param filename Filename of the shader text source, as in the
  /// `assets/shaders/src`
  /// \param config %Configuration struct
  Shader(const Device &device, const char *filename, const Config &config);
  /// Deleted copy constructor
  Shader(const Shader &) = delete;
  /// Move constructor
  Shader(Shader &&other) noexcept;

  operator SDL_GPUShader *() const noexcept { return _shader; }
  void release() noexcept;
  ~Shader() noexcept { release(); }

  /// \brief Load shader from metadata.
  static Shader fromMetadata(const Device &device, const char *filename);

private:
  SDL_GPUShader *_shader;
  SDL_GPUDevice *_device;
};

inline Shader::Shader(Shader &&other) noexcept
    : _shader(other._shader), _device(other._device) {
  other._shader = nullptr;
  other._device = nullptr;
}

/// \brief Load shader config from metadata. Metadata filename (in JSON format)
/// is inferred from the shader name.
Shader::Config loadShaderMetadata(const char *shader_name);

inline Shader Shader::fromMetadata(const Device &device,
                                   const char *shader_name) {
  auto config = loadShaderMetadata(shader_name);
  return Shader{device, shader_name, config};
}

} // namespace candlewick
