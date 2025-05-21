#include "WriteTextureToImage.h"
#include "../core/Device.h"
#include "../core/CommandBuffer.h"
#include "../core/errors.h"
#include "../third-party/stb/stb_image_write.h"
#include "../utils/PixelFormatConversion.h"

namespace candlewick {
namespace media {

  static SDL_GPUTransferBuffer *acquireBufferImpl(SDL_GPUDevice *device,
                                                  Uint32 requiredSize) {
    SDL_GPUTransferBufferCreateInfo createInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
        .size = requiredSize,
        .props = 0,
    };
    return SDL_CreateGPUTransferBuffer(device, &createInfo);
  }

  TransferBufferPool::TransferBufferPool(const Device &device)
      : _device(device) {
    // pre-allocate 8MB
    Uint32 size = 2048 * 1024 * 4;
    _buffer = acquireBufferImpl(_device, size);
    _currentBufSize = size;
  }

  void TransferBufferPool::release() noexcept {
    if (_device) {
      if (_buffer)
        SDL_ReleaseGPUTransferBuffer(_device, _buffer);
      _buffer = nullptr;
    }
    _device = nullptr;
  }

  SDL_GPUTransferBuffer *
  TransferBufferPool::acquireBuffer(Uint32 requiredSize) {
    if (_currentBufSize < requiredSize) {
      // release old buffer if it exists
      if (_buffer) {
        SDL_ReleaseGPUTransferBuffer(_device, _buffer);
      }

      // grow by 20%
      _currentBufSize = Uint32(1.2 * requiredSize);
      SDL_Log("Reallocate transfer buffer of size %u", _currentBufSize);
      _buffer = acquireBufferImpl(_device, _currentBufSize);
    }
    return _buffer;
  }

  DownloadResult downloadTexture(CommandBuffer &command_buffer,
                                 const Device &device, TransferBufferPool &pool,
                                 SDL_GPUTexture *texture,
                                 SDL_GPUTextureFormat format,
                                 const Uint16 width, const Uint16 height) {

    // pixel size, in bytes
    const Uint32 pixelSize = SDL_GPUTextureFormatTexelBlockSize(format);
    const Uint32 requiredSize = width * height * pixelSize;
    assert(requiredSize ==
           SDL_CalculateGPUTextureFormatSize(format, width, height, 1));

    SDL_GPUTransferBuffer *buffer = pool.acquireBuffer(requiredSize);

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    SDL_GPUTextureRegion source{
        .texture = texture,
        .layer = 0,
        .w = width,
        .h = height,
    };
    SDL_GPUTextureTransferInfo destination{
        .transfer_buffer = buffer,
        .offset = 0,
    };
    SDL_DownloadFromGPUTexture(copy_pass, &source, &destination);
    SDL_EndGPUCopyPass(copy_pass);
    command_buffer.submit();

    return {
        .data = reinterpret_cast<Uint32 *>(
            SDL_MapGPUTransferBuffer(device, buffer, false)),
        .format = format,
        .width = width,
        .height = height,
        .buffer = buffer,
        .payloadSize = requiredSize,
    };
  }

  void saveTextureToFile(CommandBuffer &command_buffer, const Device &device,
                         TransferBufferPool &pool, SDL_GPUTexture *texture,
                         SDL_GPUTextureFormat format, Uint16 width,
                         Uint16 height, std::string_view filename) {

    auto res = downloadTexture(command_buffer, device, pool, texture, format,
                               width, height);

    Uint32 *pixels_to_write = res.data;

    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
      bgraToRgbaConvert(pixels_to_write, res.width * res.height);
      break;
    default:
      break;
    }

    bool ret = stbi_write_png(filename.data(), int(res.width), int(res.height),
                              4, pixels_to_write, 0);

    if (!ret)
      terminate_with_message(
          "stbi_write_png() failed. Please check filename ({:s})", filename);

    SDL_UnmapGPUTransferBuffer(device, res.buffer);
  }
} // namespace media
} // namespace candlewick
