#include "VideoRecorder.h"
#include "WriteTextureToImage.h"
#include "../core/errors.h"
#include "../core/Device.h"

#include <SDL3/SDL_filesystem.h>
#include <magic_enum/magic_enum.hpp>

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

  struct VideoRecorderImpl {
    int m_width;           //< Width of incoming frames
    int m_height;          //< Height of incoming frames
    Uint32 m_frameCounter; //< Number of recorded frames

    AVFormatContext *formatContext = nullptr;
    const AVCodec *codec = nullptr;
    AVCodecContext *codecContext = nullptr;
    AVStream *videoStream = nullptr;
    SwsContext *swsContext = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *tmpFrame = nullptr;
    AVPacket *packet = nullptr;

    VideoRecorderImpl(int width, int height, std::string_view filename,
                      VideoRecorder::Settings settings);

    VideoRecorderImpl(const VideoRecorderImpl &) = delete;

    void writeFrame(const Uint8 *data, Uint32 payloadSize,
                    AVPixelFormat avPixelFormat);

    void close() noexcept;

    ~VideoRecorderImpl() noexcept { this->close(); }

    // delayed initialization, given actual input specs
    void lazyInit(AVPixelFormat inputFormat) {
      tmpFrame = allocate_frame(inputFormat, m_width, m_height);
      if (!tmpFrame)
        terminate_with_message("Failed to allocate temporary video frame.");

      swsContext =
          sws_getContext(tmpFrame->width, tmpFrame->height, inputFormat,
                         frame->width, frame->height, codecContext->pix_fmt,
                         SWS_BILINEAR, nullptr, nullptr, nullptr);
      if (!swsContext)
        terminate_with_message("Failed to create SwsContext.");
    }
  };

  void VideoRecorderImpl::close() noexcept {
    m_frameCounter = 0;
    if (!formatContext)
      return;

    av_write_trailer(formatContext);

    // close out stream
    av_frame_free(&frame);
    av_frame_free(&tmpFrame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);

    avio_closep(&formatContext->pb);
    avformat_free_context(formatContext);
    sws_freeContext(swsContext);
    formatContext = nullptr;
  }

  VideoRecorderImpl::VideoRecorderImpl(int width, int height,
                                       std::string_view filename,
                                       VideoRecorder::Settings settings)
      : m_width(width), m_height(height) {

    if (settings.outputWidth == 0)
      settings.outputWidth = width;
    if (settings.outputHeight == 0)
      settings.outputHeight = height;

    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
      terminate_with_message("Failed to find encoder for codec H264");
    }

    int ret = avformat_alloc_output_context2(&formatContext, nullptr, nullptr,
                                             filename.data());
    char errbuf[AV_ERROR_MAX_STRING_SIZE]{0};
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Could not create output context: %s", errbuf);
    }

    videoStream = avformat_new_stream(formatContext, codec);
    if (!videoStream)
      terminate_with_message("Could not allocate video stream.");

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
      terminate_with_message("Could not allocate video codec context.");
    }

    codecContext->width = settings.outputWidth;
    codecContext->height = settings.outputHeight;
    // use YUV420P as it is the most widely supported format
    // for H.264
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    codecContext->time_base = AVRational{1, settings.fps};
    codecContext->framerate = AVRational{settings.fps, 1};
    codecContext->gop_size = 10;
    codecContext->max_b_frames = 1;
    codecContext->bit_rate = settings.bitRate;

    ret = avcodec_parameters_from_context(videoStream->codecpar, codecContext);
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Couldn't copy codec params: {:s}", errbuf);
    }

    ret = avcodec_open2(codecContext, codec, nullptr);
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Couldn't open codec: {:s}", errbuf);
    }

    ret = avio_open(&formatContext->pb, filename.data(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Couldn't open output stream: {:s}", errbuf);
    }

    ret = avformat_write_header(formatContext, nullptr);
    if (ret < 0) {
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      terminate_with_message("Couldn't write output header: {:s}", errbuf);
    }

    packet = av_packet_alloc();
    if (!packet)
      terminate_with_message("Failed to allocate AVPacket");

    // principal frame
    frame = allocate_frame(codecContext->pix_fmt, codecContext->width,
                           codecContext->height);
    if (!frame)
      terminate_with_message("Failed to allocate frame.");
  }

  void VideoRecorderImpl::writeFrame(const Uint8 *data, Uint32 payloadSize,
                                     AVPixelFormat avPixelFormat) {
    assert(frame);
    int ret;
    if (!tmpFrame) {
      lazyInit(avPixelFormat);
    }

    char errbuf[AV_ERROR_MAX_STRING_SIZE]{0};
    ret = av_frame_make_writable(tmpFrame);
    if (ret < 0)
      terminate_with_message(
          "Failed to make tmpFrame writable: {:s}",
          av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));

    // copy input payload to tmp frame
    memcpy(tmpFrame->data[0], data, payloadSize);

    // ensure frame writable
    ret = av_frame_make_writable(frame);
    if (ret < 0) {
      terminate_with_message(
          "Failed to make frame writable: {:s}",
          av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));
    }
    frame->pts = m_frameCounter++;

    sws_scale(swsContext, tmpFrame->data, tmpFrame->linesize, 0, m_height,
              frame->data, frame->linesize);

    ret = avcodec_send_frame(codecContext, frame);
    if (ret < 0) {
      terminate_with_message(
          "Error sending frame {:s}",
          av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));
    }

    while (ret >= 0) {
      ret = avcodec_receive_packet(codecContext, packet);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      }
      if (ret < 0) {
        terminate_with_message(
            "Error receiving packet from encoder: {:s}",
            av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret));
      }

      av_packet_rescale_ts(packet, codecContext->time_base,
                           videoStream->time_base);
      packet->stream_index = videoStream->index;
      av_interleaved_write_frame(formatContext, packet);
      av_packet_unref(packet);
    }
  }

  // WRAPPING CLASS
  VideoRecorder::VideoRecorder(NoInitT) : _impl() {}
  VideoRecorder::VideoRecorder(VideoRecorder &&) noexcept = default;
  VideoRecorder &VideoRecorder::operator=(VideoRecorder &&) noexcept = default;

  VideoRecorder::VideoRecorder(Uint32 width, Uint32 height,
                               std::string_view filename, Settings settings)
      : _width(width), _height(height) {
    this->settings = settings;
    this->open(width, height, filename);
  }

  VideoRecorder::VideoRecorder(Uint32 width, Uint32 height,
                               std::string_view filename)
      : VideoRecorder(width, height, filename, Settings{}) {}

  void VideoRecorder::open(Uint32 width, Uint32 height,
                           std::string_view filename) {
    if (_impl)
      terminate_with_message("Recording stream already open.");

    SDL_Log("[VideoRecorder] Opening stream at %s", filename.data());
    _width = width;
    _height = height;
    _impl = std::make_unique<VideoRecorderImpl>(int(_width), int(_height),
                                                filename, settings);
  }

  Uint32 VideoRecorder::frameCounter() const { return _impl->m_frameCounter; }

  void VideoRecorder::close() noexcept {
    if (_impl)
      _impl.reset();
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

  VideoRecorder::~VideoRecorder() = default;

  void VideoRecorder::writeTextureToVideoFrame(CommandBuffer &command_buffer,
                                               const Device &device,
                                               TransferBufferPool &pool,
                                               SDL_GPUTexture *texture,
                                               SDL_GPUTextureFormat format) {

    auto res = downloadTexture(command_buffer, device, pool, texture, format,
                               Uint16(_width), Uint16(_height));

    AVPixelFormat outputFormat =
        convert_SDLTextureFormatTo_AVPixelFormat(format);
    _impl->writeFrame(reinterpret_cast<Uint8 *>(res.data), res.payloadSize,
                      outputFormat);

    SDL_UnmapGPUTransferBuffer(device, res.buffer);
  }

} // namespace media
} // namespace candlewick
