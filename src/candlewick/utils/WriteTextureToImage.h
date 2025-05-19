#pragma once

#include "../core/Core.h"
#include <SDL3/SDL_gpu.h>

namespace candlewick {
namespace media {

  struct DownloadResult {
    Uint32 *data;
    SDL_GPUTextureFormat format;
    Uint16 width;
    Uint16 height;
    SDL_GPUTransferBuffer *buffer; // used for unmapping later
    Uint32 payloadSize;
  };

  /// \brief Transfer buffer for the texture downloader.
  class TransferBufferPool {
    SDL_GPUDevice *_device = nullptr;
    SDL_GPUTransferBuffer *_buffer = nullptr;
    Uint32 _currentBufSize = 0;

  public:
    TransferBufferPool(const Device &device);
    void release() noexcept;
    ~TransferBufferPool() noexcept { this->release(); }

    SDL_GPUTransferBuffer *acquireBuffer(Uint32 requiredSize);
  };

  /// \brief Download texture to a mapped buffer.
  ///
  /// \warning The user is expected to unmap the buffer in the result struct.
  DownloadResult downloadTexture(CommandBuffer &command_buffer,
                                 const Device &device, TransferBufferPool &pool,
                                 SDL_GPUTexture *texture,
                                 SDL_GPUTextureFormat format,
                                 const Uint16 width, const Uint16 height);

  void saveTextureToFile(CommandBuffer &command_buffer, const Device &device,
                         TransferBufferPool &pool, SDL_GPUTexture *texture,
                         SDL_GPUTextureFormat format, const Uint16 width,
                         const Uint16 height, const char *filename);

} // namespace media
} // namespace candlewick
