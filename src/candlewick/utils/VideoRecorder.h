#pragma once

#ifndef CANDLEWICK_WITH_FFMPEG_SUPPORT
#error "Including this file requires candlewick to be built with FFmpeg support"
#endif
#include "../core/Core.h"
#include "../core/Tags.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>
#include <string>
#include <memory>

namespace candlewick {
namespace media {

  struct VideoRecorderImpl;

  class TransferBufferPool;

  class VideoRecorder {
    std::unique_ptr<VideoRecorderImpl> _impl;
    Uint32 _width;
    Uint32 _height;

  public:
    struct Settings {
      int fps = 30;
      // default: 2.5 Mb/s
      long bit_rate = 2500000u;
      int outputWidth;
      int outputHeight;
    } settings;

    /// \brief Constructor which will not open the file or stream.
    explicit VideoRecorder(NoInitT);
    VideoRecorder(VideoRecorder &&) noexcept;
    VideoRecorder &operator=(VideoRecorder &&) noexcept;

    void open(Uint32 width, Uint32 height, const std::string &filename);

    bool isRecording() const { return _impl != nullptr; }

    /// \brief Constructor for the video recorder.
    ///
    /// \param width Input data width.
    /// \param height Input data height.
    /// \param settings Video recording settings (fps, bitrate, output file
    /// width and height).
    ///
    /// \note If the settings' output dimensions are not set, they will
    /// automatically be set to be the input's dimensions.
    VideoRecorder(Uint32 width, Uint32 height, const std::string &filename,
                  Settings settings);

    VideoRecorder(Uint32 width, Uint32 height, const std::string &filename);

    Uint32 frameCounter() const;

    void close() noexcept;

    ~VideoRecorder();

    void writeTextureToVideoFrame(CommandBuffer &command_buffer,
                                  const Device &device,
                                  TransferBufferPool &pool,
                                  SDL_GPUTexture *texture,
                                  SDL_GPUTextureFormat format);
  };

} // namespace media
} // namespace candlewick
