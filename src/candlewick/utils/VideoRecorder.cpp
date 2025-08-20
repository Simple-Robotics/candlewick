#include "VideoRecorder.h"
#include "WriteTextureToImage.h"
#include "../core/errors.h"
#include "../core/Device.h"
#include "../core/Texture.h"

#include <SDL3/SDL_filesystem.h>
#include <magic_enum/magic_enum.hpp>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace candlewick {
namespace media {

  static AVFrame *allocate_frame(AVPixelFormat pix_fmt, int width, int height) {
    AVFrame *frame;
    int ret;

    frame = av_frame_alloc();
    if (!frame)
      return nullptr;

    frame->format = pix_fmt;
    frame->width = width;
    frame->height = height;

    char errbuf[AV_ERROR_MAX_STRING_SIZE]{0};

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
      terminate_with_message(
          "Failed to allocate frame data: %s",
          av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));

    return frame;
  }

  static AVPixelFormat
  convert_SDLTextureFormatTo_AVPixelFormat(SDL_GPUTextureFormat pixelFormat) {
    switch (pixelFormat) {
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
      return AV_PIX_FMT_BGRA;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
      return AV_PIX_FMT_RGBA;
    default:
      terminate_with_message("Unsupported SDL GPU texture format {:s}",
                             magic_enum::enum_name(pixelFormat));
    }
  }

  // IMPLEMENTING CLASS ----------------------------------------------

  struct VideoRecorderImpl {
    int m_width{0};           //< Width of incoming frames
    int m_height{0};          //< Height of incoming frames
    Uint32 m_frameCounter{0}; //< Number of recorded frames

    AVFormatContext *m_formatContext = nullptr;
    const AVCodec *m_codec = nullptr;
    AVCodecContext *m_codecContext = nullptr;
    AVStream *m_videoStream = nullptr;
    SwsContext *m_swsContext = nullptr;
    AVFrame *m_frame = nullptr;
    AVFrame *m_tmpFrame = nullptr;
    AVPacket *m_packet = nullptr;

    VideoRecorderImpl(int width, int height, std::string_view filename,
                      VideoRecorder::Settings settings);

    VideoRecorderImpl(const VideoRecorderImpl &) = delete;
    VideoRecorderImpl &operator=(const VideoRecorderImpl &) = delete;

    void writeFrame(const Uint8 *data, Uint32 payloadSize,
                    AVPixelFormat avPixelFormat);

    void close() noexcept;

    ~VideoRecorderImpl() noexcept { this->close(); }

    // delayed initialization, given actual input specs
    void lazyInit(AVPixelFormat inputFormat) {
      m_tmpFrame = allocate_frame(inputFormat, m_width, m_height);
      if (!m_tmpFrame)
        terminate_with_message("Failed to allocate temporary video frame.");

      m_swsContext = sws_getContext(m_tmpFrame->width, m_tmpFrame->height,
                                    inputFormat, m_frame->width,
                                    m_frame->height, m_codecContext->pix_fmt,
                                    SWS_BILINEAR, nullptr, nullptr, nullptr);
      if (!m_swsContext)
        terminate_with_message("Failed to create SwsContext.");
    }
  };

  void VideoRecorderImpl::close() noexcept {
    m_frameCounter = 0;
    if (!m_formatContext)
      return;

    av_write_trailer(m_formatContext);

    // close out stream
    av_frame_free(&m_frame);
    av_frame_free(&m_tmpFrame);
    av_packet_free(&m_packet);
    avcodec_free_context(&m_codecContext);

    avio_closep(&m_formatContext->pb);
    avformat_free_context(m_formatContext);
    sws_freeContext(m_swsContext);
    m_formatContext = nullptr;
  }

  VideoRecorderImpl::VideoRecorderImpl(int width, int height,
                                       std::string_view filename,
                                       VideoRecorder::Settings settings)
      : m_width(width), m_height(height) {

    assert(settings.outputWidth > 0);
    assert(settings.outputHeight > 0);

    m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!m_codec) {
      terminate_with_message("Failed to find encoder for codec H264");
    }

    int ret = avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr,
                                             filename.data());
    char errbuf[AV_ERROR_MAX_STRING_SIZE]{0};
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Could not create output context: %s", errbuf);
    }

    m_videoStream = avformat_new_stream(m_formatContext, m_codec);
    if (!m_videoStream)
      terminate_with_message("Could not allocate video stream.");

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext) {
      terminate_with_message("Could not allocate video codec context.");
    }

    m_codecContext->width = settings.outputWidth;
    m_codecContext->height = settings.outputHeight;
    // use YUV420P as it is the most widely supported format
    // for H.264
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecContext->time_base = AVRational{1, settings.fps};
    m_codecContext->framerate = AVRational{settings.fps, 1};
    m_codecContext->gop_size = 10;
    m_codecContext->max_b_frames = 1;
    m_codecContext->bit_rate = settings.bitRate;

    ret = avcodec_parameters_from_context(m_videoStream->codecpar,
                                          m_codecContext);
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Couldn't copy codec params: {:s}", errbuf);
    }

    ret = avcodec_open2(m_codecContext, m_codec, nullptr);
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Couldn't open codec: {:s}", errbuf);
    }

    ret = avio_open(&m_formatContext->pb, filename.data(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Couldn't open output stream: {:s}", errbuf);
    }

    ret = avformat_write_header(m_formatContext, nullptr);
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Couldn't write output header: {:s}", errbuf);
    }

    m_packet = av_packet_alloc();
    if (!m_packet)
      terminate_with_message("Failed to allocate AVPacket");

    // principal frame
    m_frame = allocate_frame(m_codecContext->pix_fmt, m_codecContext->width,
                             m_codecContext->height);
    if (!m_frame)
      terminate_with_message("Failed to allocate frame.");
  }

  void VideoRecorderImpl::writeFrame(const Uint8 *data, Uint32 payloadSize,
                                     AVPixelFormat avPixelFormat) {
    assert(m_frame);
    int ret;
    if (!m_tmpFrame) {
      lazyInit(avPixelFormat);
    }

    char errbuf[AV_ERROR_MAX_STRING_SIZE]{0};
    ret = av_frame_make_writable(m_tmpFrame);
    if (ret < 0)
      terminate_with_message(
          "Failed to make tmpFrame writable: {:s}",
          av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));

    // copy input payload to tmp frame
    memcpy(m_tmpFrame->data[0], data, payloadSize);

    // ensure frame writable
    ret = av_frame_make_writable(m_frame);
    if (ret < 0) {
      terminate_with_message(
          "Failed to make frame writable: {:s}",
          av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));
    }
    m_frame->pts = m_frameCounter++;

    sws_scale(m_swsContext, m_tmpFrame->data, m_tmpFrame->linesize, 0, m_height,
              m_frame->data, m_frame->linesize);

    ret = avcodec_send_frame(m_codecContext, m_frame);
    if (ret < 0) {
      terminate_with_message(
          "Error sending frame {:s}",
          av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));
    }

    while (ret >= 0) {
      ret = avcodec_receive_packet(m_codecContext, m_packet);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      }
      if (ret < 0) {
        terminate_with_message(
            "Error receiving packet from encoder: {:s}",
            av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));
      }

      av_packet_rescale_ts(m_packet, m_codecContext->time_base,
                           m_videoStream->time_base);
      m_packet->stream_index = m_videoStream->index;
      av_interleaved_write_frame(m_formatContext, m_packet);
      av_packet_unref(m_packet);
    }
  }

  // WRAPPING CLASS --------------------------------------------------

  VideoRecorder::VideoRecorder(NoInitT) : m_impl() {}
  VideoRecorder::VideoRecorder(VideoRecorder &&) noexcept = default;
  VideoRecorder &VideoRecorder::operator=(VideoRecorder &&) noexcept = default;

  VideoRecorder::VideoRecorder(Uint32 width, Uint32 height,
                               std::string_view filename, Settings settings)
      : m_width(width), m_height(height) {
    this->open(width, height, filename, std::move(settings));
  }

  VideoRecorder::VideoRecorder(Uint32 width, Uint32 height,
                               std::string_view filename)
      : VideoRecorder(width, height, filename, Settings{}) {}

  void VideoRecorder::open(Uint32 width, Uint32 height,
                           std::string_view filename, Settings settings) {
    std::filesystem::path pt{filename};
    auto ext = pt.extension();
    if (ext.empty() || (ext != ".mp4")) {
      terminate_with_message(
          "Filename {:s} does not end with extension \'.mp4\'.", filename);
    }

    if (m_impl) {
      terminate_with_message("Recording stream already open.");
    }

    spdlog::info(
        "[{}] Opening video stream at {:s} (fps = {:d}, bitrate = {:d}Mbps)",
        typeid(*this), filename, settings.fps, settings.bitRate / 1000);
    m_width = width;
    m_height = height;
    if (settings.outputWidth == 0)
      settings.outputWidth = int(m_width);
    if (settings.outputHeight == 0)
      settings.outputHeight = int(m_height);
    m_impl = std::make_unique<VideoRecorderImpl>(int(m_width), int(m_height),
                                                 filename, settings);
  }

  Uint32 VideoRecorder::frameCounter() const { return m_impl->m_frameCounter; }

  void VideoRecorder::close() noexcept {
    spdlog::info("[{}] Closing recording stream, wrote {:d} frames.",
                 typeid(*this), frameCounter());
    if (m_impl)
      m_impl.reset();
  }

  VideoRecorder::~VideoRecorder() = default;

  void VideoRecorder::writeTextureToFrame(CommandBuffer &command_buffer,
                                          const Device &device,
                                          TransferBufferPool &pool,
                                          SDL_GPUTexture *texture,
                                          SDL_GPUTextureFormat format) {

    auto res = downloadTexture(command_buffer, device, pool, texture, format,
                               Uint16(m_width), Uint16(m_height));

    AVPixelFormat outputFormat =
        convert_SDLTextureFormatTo_AVPixelFormat(format);
    m_impl->writeFrame(reinterpret_cast<Uint8 *>(res.data), res.payloadSize,
                       outputFormat);

    SDL_UnmapGPUTransferBuffer(device, res.buffer);
  }

  void VideoRecorder::writeTextureToFrame(CommandBuffer &command_buffer,
                                          const Device &device,
                                          TransferBufferPool &pool,
                                          const Texture &texture) {
    this->writeTextureToFrame(command_buffer, device, pool, texture,
                              texture.format());
  }

} // namespace media
} // namespace candlewick
