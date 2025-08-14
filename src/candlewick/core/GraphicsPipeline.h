#pragma once

#include "Core.h"
#include "Tags.h"
#include "errors.h"
#include <SDL3/SDL_gpu.h>

namespace candlewick {

/// \brief Class representing a graphics pipeline.
///
/// The GraphicsPipeline is a RAII wrapper around the \c SDL_GPUGraphicsPipeline
/// handle.
class GraphicsPipeline {
  SDL_GPUDevice *m_device{nullptr};
  SDL_GPUGraphicsPipeline *m_pipeline{nullptr};
  SDL_GPUPrimitiveType m_primitiveType;

public:
  GraphicsPipeline(NoInitT) {}
  GraphicsPipeline(SDL_GPUDevice *device,
                   SDL_GPUGraphicsPipelineCreateInfo pipeline_desc,
                   const char *name)
      : m_device(device)
      , m_pipeline(nullptr)
      , m_primitiveType(pipeline_desc.primitive_type) {
    if (name && !pipeline_desc.props) {
      pipeline_desc.props = SDL_CreateProperties();
      SDL_SetStringProperty(pipeline_desc.props,
                            SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING,
                            name);
    }
    m_pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipeline_desc);
    if (!m_pipeline) {
      terminate_with_message("Failed to create graphics pipeline: {:s}",
                             SDL_GetError());
    }
  }

  bool initialized() const noexcept { return m_pipeline; }
  SDL_GPUGraphicsPipeline *handle() const noexcept { return m_pipeline; }

  GraphicsPipeline(const GraphicsPipeline &) = delete;
  GraphicsPipeline(GraphicsPipeline &&other) noexcept
      : m_device(other.m_device), m_pipeline(other.m_pipeline) {
    other.m_device = nullptr;
    other.m_pipeline = nullptr;
  }

  GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;
  GraphicsPipeline &operator=(GraphicsPipeline &&other) noexcept {
    if (this != &other) {
      this->release(); // release if we've got managed resources
      m_device = other.m_device;
      m_pipeline = other.m_pipeline;
      m_primitiveType = other.m_primitiveType;

      other.m_device = nullptr;
      other.m_pipeline = nullptr;
    }
    return *this;
  }

  auto primitiveType() const noexcept { return m_primitiveType; }

  void bind(SDL_GPURenderPass *render_pass) const noexcept {
    SDL_BindGPUGraphicsPipeline(render_pass, m_pipeline);
  }

  void release() noexcept {
    if (m_device && m_pipeline)
      SDL_ReleaseGPUGraphicsPipeline(m_device, m_pipeline);
    m_pipeline = nullptr;
  }

  ~GraphicsPipeline() noexcept { this->release(); }
};

} // namespace candlewick
