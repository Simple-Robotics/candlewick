#include "Shader.h"
#include "Device.h"
#include "errors.h"

#define JSON_NO_IO
#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

namespace candlewick {

std::string g_shader_dir = g_default_shader_dir;

void setShadersDirectory(const char *path) { g_shader_dir = path; }
const char *currentShaderDirectory() { return g_shader_dir.c_str(); }

SDL_GPUShaderStage detect_shader_stage(const char *filename) {
  SDL_GPUShaderStage stage;
  if (SDL_strstr(filename, ".vert"))
    stage = SDL_GPU_SHADERSTAGE_VERTEX;
  else if (SDL_strstr(filename, ".frag"))
    stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
  else {
    stage = SDL_GPUShaderStage(-1);
  }
  return stage;
}

struct ShaderCode {
  Uint8 *data;
  size_t size;
  ShaderCode(Uint8 *d, size_t s) : data(d), size(s) {}
  ShaderCode(const ShaderCode &) = delete;
  ShaderCode(ShaderCode &&) = delete;
  ShaderCode &operator=(const ShaderCode &) = delete;
  ShaderCode &operator=(ShaderCode &&) = delete;
  ~ShaderCode() noexcept { SDL_free(data); }
};

ShaderCode loadShaderFile(const char *filename, const char *shader_ext) {
  char shader_path[256];
  SDL_snprintf(shader_path, sizeof(shader_path), "%s/%s.%s",
               g_shader_dir.c_str(), filename, shader_ext);

  size_t code_size;
  void *code = SDL_LoadFile(shader_path, &code_size);
  if (!code) {
    throw RAIIException(SDL_GetError());
  }
  return ShaderCode{reinterpret_cast<Uint8 *>(code), code_size};
}

Shader::Shader(const Device &device, const char *filename, const Config &config)
    : _shader(nullptr), _device(device) {
  _stage = detect_shader_stage(filename);
  if (_stage < 0)
    terminate_with_message("Failed to infer shader stage for filename {:s}",
                           filename);

  SDL_GPUShaderFormat supported_formats = device.shaderFormats();
  SDL_GPUShaderFormat target_format = SDL_GPU_SHADERFORMAT_INVALID;
  const char *shader_ext;
  const char *entry_point;

  if (supported_formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    target_format = SDL_GPU_SHADERFORMAT_SPIRV;
    shader_ext = "spv";
    entry_point = "main";
  } else if (supported_formats & SDL_GPU_SHADERFORMAT_MSL) {
    target_format = SDL_GPU_SHADERFORMAT_MSL;
    shader_ext = "msl";
    entry_point = "main_0";
  } else {
    throw RAIIException(
        "Failed to load shader: no available supported shader format.");
  }

  ShaderCode shader_code = loadShaderFile(filename, shader_ext);

  SDL_GPUShaderCreateInfo info{
      .code_size = shader_code.size,
      .code = shader_code.data,
      .entrypoint = entry_point,
      .format = target_format,
      .stage = _stage,
      .num_samplers = config.samplers,
      .num_storage_textures = config.storage_textures,
      .num_storage_buffers = config.storage_buffers,
      .num_uniform_buffers = config.uniform_buffers,
      .props = 0U,
  };
  if (!(_shader = SDL_CreateGPUShader(device, &info))) {
    throw RAIIException(SDL_GetError());
  }
}

void Shader::release() noexcept {
  if (_device && _shader) {
    SDL_ReleaseGPUShader(_device, _shader);
    _shader = nullptr;
  }
}

Shader::Config loadShaderMetadata(const char *filename) {
  auto data = loadShaderFile(filename, "json");
  auto json = nlohmann::json::parse(data.data, data.data + data.size);

  Shader::Config config{};
  for (const auto &param : json.at("parameters")) {
    const auto &kind =
        param.at("type").at("kind").get_ref<const std::string &>();
    if (kind == "constantBuffer") {
      config.uniform_buffers++;
    } else if (kind == "resource" &&
               param.at("type").value("combined", false)) {
      // Slang emits "resource" + "combined": true for Sampler2D /
      // Sampler2DShadow. Each such entry corresponds to one
      // SDL_GPUTextureSamplerBinding slot.
      config.samplers++;
    } else if (kind == "rwTexture") {
      config.storage_textures++;
    } else if (kind == "structuredBuffer" || kind == "rwStructuredBuffer") {
      config.storage_buffers++;
    }
  }
  return config;
}

} // namespace candlewick
