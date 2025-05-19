#include "WriteTextureToImage.h"
#include "../core/Device.h"
#include "../core/CommandBuffer.h"
#include "../third-party/stb_image_write.h"
#include "../utils/PixelFormatConversion.h"

namespace candlewick::media {

static SDL_GPUTransferBuffer *acquireBufferImpl(SDL_GPUDevice *device,
                                                Uint32 requiredSize) {
  SDL_GPUTransferBufferCreateInfo createInfo{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
      .size = requiredSize,
      .props = 0,
  };
  return SDL_CreateGPUTransferBuffer(device, &createInfo);
}

TransferBufferPool::TransferBufferPool(const Device &device) : _device(device) {
  // pre-allocate 4MB
  Uint32 size = 1024 * 1024 * 4;
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

SDL_GPUTransferBuffer *TransferBufferPool::acquireBuffer(Uint32 requiredSize) {
  if (_currentBufSize < requiredSize) {
    // 20% boost
    requiredSize = Uint32(1.2 * requiredSize);
    SDL_Log("TransferBufferPool: re-allocating %u bytes (increase from %u)",
            requiredSize, _currentBufSize);
    _buffer = acquireBufferImpl(_device, requiredSize);
    _currentBufSize = requiredSize;
  }
  return _buffer;
}

DownloadResult downloadTexture(CommandBuffer &command_buffer,
                               const Device &device, TransferBufferPool &pool,
                               SDL_GPUTexture *texture,
                               SDL_GPUTextureFormat format, const Uint16 width,
                               const Uint16 height) {

  // pixel size, in bytes
  const Uint32 pixelSize = SDL_GPUTextureFormatTexelBlockSize(format);
  const Uint32 requiredSize = width * height * pixelSize;
  assert(requiredSize ==
         SDL_CalculateGPUTextureFormatSize(format, width, height, 1));

  SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);

  SDL_GPUTextureRegion source{
      .texture = texture,
      .layer = 0,
      .w = width,
      .h = height,
  };

  SDL_GPUTransferBuffer *download_transfer_buffer =
      pool.acquireBuffer(requiredSize);

  SDL_GPUTextureTransferInfo destination{
      .transfer_buffer = download_transfer_buffer,
      .offset = 0,
  };

  SDL_DownloadFromGPUTexture(copy_pass, &source, &destination);
  SDL_EndGPUCopyPass(copy_pass);

  command_buffer.submit();

  auto *raw_pixels = reinterpret_cast<Uint32 *>(
      SDL_MapGPUTransferBuffer(device, download_transfer_buffer, false));

  return {
      .data = raw_pixels,
      .format = format,
      .width = width,
      .height = height,
      .buffer = download_transfer_buffer,
      .payloadSize = requiredSize,
  };
}

void writeToFile(CommandBuffer &command_buffer, const Device &device,
                 TransferBufferPool &pool, SDL_GPUTexture *texture,
                 SDL_GPUTextureFormat format, Uint16 width, Uint16 height,
                 const char *filename) {

  auto res = downloadTexture(command_buffer, device, pool, texture, format,
                             width, height);
  auto *raw_pixels = res.data;
  auto *rgba_pixels = (Uint32 *)SDL_malloc(res.payloadSize);
  if (format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) {
    bgraToRgbaConvert(raw_pixels, rgba_pixels, res.width * res.height);
  } else {
    SDL_memcpy(rgba_pixels, raw_pixels, res.payloadSize);
  }

  stbi_write_png(filename, int(res.width), int(res.height), 4, rgba_pixels, 0);
  std::free(rgba_pixels);

  SDL_UnmapGPUTransferBuffer(device, res.buffer);
}

#ifdef CANDLEWICK_WITH_FFMPEG_SUPPORT
void videoWriteTextureToFrame(CommandBuffer &command_buffer,
                              const Device &device, TransferBufferPool &pool,
                              VideoRecorder &recorder, SDL_GPUTexture *texture,
                              SDL_GPUTextureFormat format, const Uint16 width,
                              const Uint16 height) {
  assert(recorder.initialized());

  auto res = downloadTexture(command_buffer, device, pool, texture, format,
                             width, height);

  Uint8 *raw_data = reinterpret_cast<Uint8 *>(res.data);
  recorder.writeFrame(raw_data, res.payloadSize, format);

  SDL_UnmapGPUTransferBuffer(device, res.buffer);
}
#endif

} // namespace candlewick::media
