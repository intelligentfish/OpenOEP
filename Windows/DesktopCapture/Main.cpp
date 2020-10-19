#include <winsock2.h>

#include "CArgs.h"
#include "CGlogInitializer.h"
#include "CSignal.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <x265.h>
#ifdef __cplusplus
}
#endif
#include <glog/logging.h>
#include <windows.h>

#include <boost/lexical_cast.hpp>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>

//#pragma comment(lib, "ws2_32")

// 桌面捕获器
struct DesktopCapture {
  // 显示模块信息
#define SHOW_MODULE_INFO(OS, MODULE)                          \
  OS << "module: " << #MODULE << '\n'                         \
     << "version: " << MODULE##_version() << '\n'             \
     << "configuration: " << MODULE##_configuration() << '\n' \
     << "license: " << MODULE##_license() << '\n'             \
     << std::endl

  // 编码器类型
  enum EncoderType : int { X264, X265 };

  // 错误码
  enum ErrorCode : int {
    Ok,
    AVFindInputFormat,
    AVFormatOpenInput,
    AVFormatFindStreamInfo,
    VideoStreamIndexAndAudioStreamIndex,
    AVCodecFindDecoder,
    AVCodecParametersToContext,
    AVCodecOpen2,
    AVReadFrame,
    CSPNotSupported,
  };

  // 单例
  static DesktopCapture& instance() {
    static DesktopCapture object;
    return object;
  }
  // 显示模块信息
  static void showModulesInfo(std::ostream& os = std::cout) {
    SHOW_MODULE_INFO(os, avcodec);
    SHOW_MODULE_INFO(os, avdevice);
    SHOW_MODULE_INFO(os, avformat);
    SHOW_MODULE_INFO(os, avfilter);
    SHOW_MODULE_INFO(os, avutil);
  }
  // 记录AV错误信息
  static std::ostream& logAVError(int ec, std::ostream& os = std::cout) {
    std::string errStr(AV_ERROR_MAX_STRING_SIZE, '\0');
    os << av_make_error_string(&errStr.at(0), AV_ERROR_MAX_STRING_SIZE, ec);
    return os;
  }
  // 析构方法
  virtual ~DesktopCapture() {
    if (m_fmtCtx) avformat_free_context(m_fmtCtx);
    if (m_x265Encoder) x265_encoder_close(m_x265Encoder);
    x265_cleanup();
  }
  // 设置目标宽度
  DesktopCapture& setDstWidth(int width) {
    m_dstWidth = width;
    return *this;
  }
  // 设置目标高度
  DesktopCapture& setDstHeight(int height) {
    m_dstHeight = height;
    return *this;
  }
  // 设置目标像素格式
  DesktopCapture& setDstPixFormat(AVPixelFormat fmt) {
    m_dstPixelFormat = fmt;
    return *this;
  }
  // 设置帧率
  DesktopCapture& setFrameRate(int frameRate) {
    m_frameRate = frameRate;
    return *this;
  }
  // 设置编码类型
  DesktopCapture& setEncoderType(EncoderType encoderType) {
    m_encoderType = encoderType;
    return *this;
  }
  // 停止
  DesktopCapture& stop() {
    m_stop = true;
    return *this;
  }
  // 捕获
  ErrorCode capture() {
    static const auto INPUT_FORMAT = "gdigrab";
    static const auto INPUT_URL = "desktop";
    auto ec = ErrorCode::Ok;
    AVInputFormat* inputFmt = nullptr;
    do {
      inputFmt = av_find_input_format(INPUT_FORMAT);
      if (nullptr == inputFmt) {
        LOG(ERROR) << "av_find_input_format error";
        ec = ErrorCode::AVFindInputFormat;
        break;
      }
      AVDictionary* dict = nullptr;
      av_dict_set(&dict, "framerate",
                  boost::lexical_cast<std::string>(m_frameRate).c_str(), NULL);
      if (avformat_open_input(&m_fmtCtx, INPUT_URL, inputFmt, &dict)) {
        LOG(ERROR) << "avformat_open_input error";
        ec = ErrorCode::AVFormatOpenInput;
        break;
      }
      if (0 > avformat_find_stream_info(m_fmtCtx, nullptr)) {
        LOG(ERROR) << "avformat_find_stream_info error";
        ec = ErrorCode::AVFormatFindStreamInfo;
        break;
      }
      auto videoIndex = -1;
      auto audioIndex = -1;
      for (unsigned int i = 0; i < m_fmtCtx->nb_streams; i++) {
        switch (m_fmtCtx->streams[i]->codecpar->codec_type) {
          case AVMediaType::AVMEDIA_TYPE_VIDEO: {
            videoIndex = i;
          } break;
          case AVMediaType::AVMEDIA_TYPE_AUDIO: {
            audioIndex = i;
          } break;
        }
      }
      if (-1 == videoIndex && -1 == audioIndex) {
        LOG(ERROR) << "video stream index and audio stream index are not found";
        ec = ErrorCode::VideoStreamIndexAndAudioStreamIndex;
        break;
      }
      auto codecPair = m_fmtCtx->streams[videoIndex]->codecpar;
      auto decoder = avcodec_find_decoder(codecPair->codec_id);
      if (nullptr == decoder) {
        LOG(ERROR) << "avcodec_find_decoder error";
        ec = ErrorCode::AVCodecFindDecoder;
        break;
      }
      auto ctx3 = avcodec_alloc_context3(decoder);
      if (0 > avcodec_parameters_to_context(ctx3, codecPair)) {
        LOG(ERROR) << "avcodec_parameters_to_context error";
        ec = ErrorCode::AVCodecParametersToContext;
        break;
      }
      if (0 > avcodec_open2(ctx3, decoder, nullptr)) {
        LOG(ERROR) << "avcodec_open2 error";
        ec = ErrorCode::AVCodecOpen2;
        break;
      }
      auto dstW = ctx3->width;
      auto dstH = ctx3->height;
      if (0 < m_dstWidth && m_dstWidth < ctx3->width) dstW = m_dstWidth;
      if (0 < m_dstHeight && m_dstHeight < ctx3->height) dstH = m_dstHeight;
      auto dstFmt = AVPixelFormat::AV_PIX_FMT_YUV420P;
      if (dstFmt != m_dstPixelFormat) dstFmt = m_dstPixelFormat;
      LOG(INFO) << "dst width: " << dstW << ", dst height: " << dstH;
      auto frameYUV = av_frame_alloc();
      av_image_fill_arrays(
          frameYUV->data, frameYUV->linesize,
          (uint8_t*)av_malloc(av_image_get_buffer_size(dstFmt, dstW, dstH, 1) *
                              sizeof(uint8_t)),
          dstFmt, dstW, dstH, 1);
      auto swsCtx =
          sws_getContext(ctx3->width, ctx3->height, ctx3->pix_fmt, dstW, dstH,
                         dstFmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
      auto ec = 0;
      auto height = 0;
      auto packet = (AVPacket*)av_malloc(sizeof(AVPacket));
      auto frameDesktop = av_frame_alloc();

      switch (m_encoderType) {
        case EncoderType::X265: {
          x265Setup(dstW, dstH);
        } break;
        case EncoderType::X264: {
        } break;
      }

      while (!m_stop) {
        if (0 > (ec = av_read_frame(m_fmtCtx, packet))) {
          logAVError(ec, LOG(ERROR) << "av_read_frame error: ");
          ec = ErrorCode::AVReadFrame;
          break;
        }
        if (0 == (ec = avcodec_send_packet(ctx3, packet))) {
          while (0 == (ec = avcodec_receive_frame(ctx3, frameDesktop))) {
            height = sws_scale(swsCtx, frameDesktop->data,
                               frameDesktop->linesize, 0, frameDesktop->height,
                               frameYUV->data, frameYUV->linesize);
            switch (m_encoderType) {
              case EncoderType::X265: {
                x265Encode(frameYUV, X265_CSP_I420, dstW);
              } break;
              case EncoderType::X264: {
              } break;
            }
          }
          if (AVERROR(EAGAIN) != ec)
            logAVError(ec, LOG(ERROR) << "avcodec_receive_frame error: ");
        } else {
          if (AVERROR_EOF == ec)
            m_stop = true;
          else if (AVERROR(EAGAIN) == ec)
            continue;
          logAVError(ec, LOG(ERROR) << "avcodec_send_packet error: ");
        }
        av_packet_unref(packet);
      }

      switch (m_encoderType) {
        case EncoderType::X265: {
          x265Flush();
        } break;
        case EncoderType::X264: {
        } break;
      }

      if (frameDesktop) av_frame_free(&frameDesktop);
      if (frameYUV) av_frame_free(&frameYUV);
      if (packet) av_free(packet);
    } while (false);
    return ec;
  }

 private:
  // 构造方法
  DesktopCapture()
      : m_stop(),
        m_dstWidth(),
        m_dstHeight(),
        m_frameRate(),
        m_dstPixelFormat(AVPixelFormat::AV_PIX_FMT_YUV420P),
        m_fmtCtx(),
        m_encoderType(EncoderType::X265),
        m_x265Picture(),
        m_x265Encoder(),
        m_x265Nal() {
    avdevice_register_all();
    m_fmtCtx = avformat_alloc_context();
    if (nullptr == m_fmtCtx)
      throw std::runtime_error("avformat_alloc_context return nullptr");
  }
  // x265设置
  void x265Setup(int sourceWidth, int sourceHeight) {
    auto x265Param = x265_param_alloc();
    x265_param_default(x265Param);
    x265Param->bRepeatHeaders = 1;
    x265Param->internalCsp = X265_CSP_I420;
    x265Param->sourceWidth = sourceWidth;
    x265Param->sourceHeight = sourceHeight;
    x265Param->fpsNum = m_frameRate;
    x265Param->fpsDenom = 1;
    x265Param->logLevel = X265_LOG_NONE;
    x265Param->frameNumThreads = 1;  // 限制CPU使用率

    // auto ret = x265_param_apply_profile(x265Param, x265_profile_names[1]);
    // if (0 > ret) LOG(ERROR) << "x265_param_apply_profile error:" << ret;

    m_x265Encoder = x265_encoder_open(x265Param);
    if (nullptr == m_x265Encoder) throw std::runtime_error("x265_encoder_open");

    m_x265Picture = x265_picture_alloc();
    x265_picture_init(x265Param, m_x265Picture);

    x265_param_free(x265Param);
  }
  // x265编码
  ErrorCode x265Encode(AVFrame* frameYUV, int internalCsp, int width) {
    switch (internalCsp) {
      case X265_CSP_I420: {
        m_x265Picture->planes[0] = frameYUV->data[0];
        m_x265Picture->planes[1] = frameYUV->data[1];
        m_x265Picture->planes[2] = frameYUV->data[2];
        m_x265Picture->stride[0] = width;
        m_x265Picture->stride[1] = width / 2;
        m_x265Picture->stride[2] = width / 2;
      } break;
      case X265_CSP_I444: {
      } break;
      default: {
        return ErrorCode::CSPNotSupported;
      }
    }

    // 获取SPS&&PPS用于初始化解码器，在生命周期内可保持唯一，x265Param->bRepeatHeaders可设置为0，让不在产生IDR帧
    // int x265_encoder_headers(x265_encoder*, x265_nal * *pp_nal,
    //                         uint32_t * pi_nal);

    unsigned int nalNumber = 0;
    auto ret = x265_encoder_encode(m_x265Encoder, &m_x265Nal, &nalNumber,
                                   m_x265Picture, nullptr);
    if (0 > ret) LOG(ERROR) << "x265_encoder_encode error: " << ret;
    return ErrorCode::Ok;
  }
  // x265刷新编码器
  void x265Flush() {
    auto ret = 0;
    unsigned int nalNumber = 0;
    while (true) {
      if (0 == (ret = x265_encoder_encode(m_x265Encoder, &m_x265Nal, &nalNumber,
                                          nullptr, nullptr)))
        break;
    }
  }

  bool m_stop;
  int m_dstWidth;
  int m_dstHeight;
  int m_frameRate;
  AVPixelFormat m_dstPixelFormat;
  AVFormatContext* m_fmtCtx;

  EncoderType m_encoderType;
  x265_picture* m_x265Picture;
  x265_encoder* m_x265Encoder;
  x265_nal* m_x265Nal;
};

int main(int argc, char** argv, char** env) {
  CGlogInitializer glogInitializer(*argv);
  CArgs::showArg(argc, argv, LOG(INFO));
  CArgs::showEnv(env, LOG(INFO));
  CSignal::instance().add(
      std::set<int>{SIGINT, SIGTERM, SIGABRT, SIGBREAK}, [&](int sig) {
        LOG(ERROR) << "caught signal: " << sig << ", stop capture";
        DesktopCapture::instance().stop();
      });
  DesktopCapture::showModulesInfo(LOG(INFO));
  auto ec = DesktopCapture::instance()
                .setFrameRate(25)
                .setEncoderType(DesktopCapture::EncoderType::X265)
                .capture();
  LOG(ERROR) << "capture desktop exit with: " << ec;
  return 0;
}