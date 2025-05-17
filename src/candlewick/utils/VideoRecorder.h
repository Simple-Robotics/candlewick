#pragma once

#ifndef CANDLEWICK_WITH_FFMPEG_SUPPORT
#error "Including this file requires candlewick to be built with FFmpeg support"
#endif
#include "../core/Tags.h"

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>
#include <string>
#include <memory>

namespace candlewick {
namespace media {

  struct VideoRecorderImpl;

  class VideoRecorder {
  private:
    std::unique_ptr<VideoRecorderImpl> impl_;

  public:
    struct Settings {
      int fps = 30;
      // default: 2.5 Mb/s
      long bit_rate = 2500000u;
      int outputWidth;
      int outputHeight;
    };

    /// \brief Constructor which will not open the file or stream.
    explicit VideoRecorder(NoInitT);
    VideoRecorder(VideoRecorder &&);
    VideoRecorder &operator=(VideoRecorder &&);

    bool initialized() const { return impl_ != nullptr; }
    VideoRecorder(Uint32 width, Uint32 height, const std::string &filename,
                  Settings settings);

    VideoRecorder(Uint32 width, Uint32 height, const std::string &filename);

    Uint32 frameCounter() const;
    void writeFrame(const Uint8 *data, size_t payloadSize,
                    SDL_GPUTextureFormat pixelFormat);
    ~VideoRecorder();
  };

} // namespace media
} // namespace candlewick
