#pragma once

#ifndef CANDLEWICK_WITH_FFMPEG_SUPPORT
#error "Including this file requires candlewick to be built with FFmpeg support"
#endif
#include "../core/Core.h"
#include "../core/Tags.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>
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
      int bitRate = 2'500'000u;
      int outputWidth = 0;
      int outputHeight = 0;
    };

    /// \brief Constructor which will not open the file or stream.
    explicit VideoRecorder(NoInitT);
    VideoRecorder(VideoRecorder &&) noexcept;
    VideoRecorder &operator=(VideoRecorder &&) noexcept;

    /// \brief Open the recording stream.
    ///
    /// \param width Input data width.
    /// \param height Input data height.
    /// \param filename Filename to open the outut stream at.
    void open(Uint32 width, Uint32 height, std::string_view filename,
              Settings settings);

    /// \brief Returns whether the recording stream is open.
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
    /// \sa open()
    VideoRecorder(Uint32 width, Uint32 height, std::string_view filename,
                  Settings settings);

    VideoRecorder(Uint32 width, Uint32 height, std::string_view filename);

    /// \brief Current number of recorded frames.
    Uint32 frameCounter() const;

    /// \brief Close the recording stream.
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
