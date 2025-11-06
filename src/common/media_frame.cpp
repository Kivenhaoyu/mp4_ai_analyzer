//
//  media_frame.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#include "media_frame.h"
#include <stdexcept>
#include <cstring>

// 仅在实现中引入FFmpeg头文件（对外隐藏依赖）
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h> // 用于时间基准转换
}

// ------------------------------
// 像素格式转换工具
// ------------------------------
PixelFormat MediaFrame::avPixelFormatToCustom(int av_fmt) {
    switch (av_fmt) {
        case AV_PIX_FMT_YUV420P:  return PixelFormat::YUV420P;
        case AV_PIX_FMT_BGR24:    return PixelFormat::BGR24;
        case AV_PIX_FMT_RGB24:    return PixelFormat::RGB24;
        case AV_PIX_FMT_UYVY422:  return PixelFormat::UYVY422;
        default:                  return PixelFormat::UNKNOWN;
    }
}

int MediaFrame::customPixelFormatToAV(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::YUV420P: return AV_PIX_FMT_YUV420P;
        case PixelFormat::BGR24:   return AV_PIX_FMT_BGR24;
        case PixelFormat::RGB24:   return AV_PIX_FMT_RGB24;
        case PixelFormat::UYVY422: return AV_PIX_FMT_UYVY422;
        default:                   return AV_PIX_FMT_NONE;
    }
}

std::string pixelFormatToString(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::YUV420P: return "YUV420P";
        case PixelFormat::BGR24:   return "BGR24";
        case PixelFormat::RGB24:   return "RGB24";
        case PixelFormat::UYVY422: return "UYVY422";
        default:                   return "UNKNOWN";
    }
}

std::string SampleFormatToString(SampleFormat fmt) {
    switch (fmt) {
        case SampleFormat::S16: return "S16";
        case SampleFormat::FLT: return "FLT";
        case SampleFormat::S32: return "S32";
        case SampleFormat::U8:  return "U8";
        default:                return "UNKNOWN";
    }
}



std::shared_ptr<MediaFrame> MediaFrame::createFromAVFrame(AVFrame* av_frame) {
    if (!av_frame) {
        throw std::invalid_argument("createFromAVFrame failed: av_frame is null");
    }
    return std::shared_ptr<MediaFrame>(new MediaFrame(av_frame));
}

std::shared_ptr<MediaFrame> MediaFrame::createEmpty(int width, int height, PixelFormat fmt) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("MediaFrame createEmpty failed: width or height <= 0");
    }
    
    if (fmt == PixelFormat::UNKNOWN) {
        throw std::invalid_argument("createEmpty failed: unknown pixel format");
    }
    
    return std::shared_ptr<MediaFrame>(new MediaFrame(width,height,fmt));
    
}

MediaFrame::MediaFrame(AVFrame* av_frame):av_frame_(av_frame) {
    if (!av_frame_) {
        throw std::invalid_argument("MediaFrame constructor failed: av_frame is null");
    }
    // 提取基础属性
    width_ = av_frame_->width;
    height_ = av_frame_->height;
    format_ = avPixelFormatToCustom(av_frame_->format);
    pts_ = av_frame->pts; // 初始化为 AVFrame 的 PTS
    
    // 初始化数据指针
    initDataPointers();
}

MediaFrame::MediaFrame(int width, int height, PixelFormat fmt):width_(width),height_(height),format_(fmt) {
    av_frame_ = av_frame_alloc();
    if(!av_frame_) {
        throw std::bad_alloc();
    }
    
    av_frame_->width = width_;
    av_frame_->height = height_;
    av_frame_->format = customPixelFormatToAV(fmt);
    
    int ret = av_frame_get_buffer(av_frame_,32);
    if (ret < 0) {
        av_frame_free(&av_frame_);
        throw std::runtime_error("av_frame_get_buffer failed: " + std::string(av_err2str(ret)));
    }
    
    // 初始化数据指针
    initDataPointers();
}

MediaFrame::~MediaFrame() {
    releaseResources();
}

MediaFrame::MediaFrame(MediaFrame&& other) noexcept :
width_(other.width_),
height_(other.height_),
format_(other.format_),
pts_(other.pts_),
pts_ms_(other.pts_ms_),
time_base_num_(other.time_base_num_),
time_base_den_(other.time_base_den_),
data_(std::move(other.data_)),
linesize_(std::move(other.linesize_)),
av_frame_(other.av_frame_) {
    other.av_frame_ = nullptr;
}

MediaFrame& MediaFrame::operator=(MediaFrame&& other) noexcept {
    if (this != &other) {
        // 销毁原始数据
        releaseResources();
        
        // 赋值新的数据
        width_ = other.width_;
        height_ = other.height_;
        format_ = other.format_;
        pts_ = other.pts_;
        pts_ms_ = other.pts_ms_;
        time_base_num_ = other.time_base_num_;
        time_base_den_ = other.time_base_den_;
        data_ = std::move(other.data_);
        linesize_ = std::move(other.linesize_);
        av_frame_ = other.av_frame_;
        
        other.av_frame_ = nullptr;
    }
    return *this;
}


void MediaFrame::initDataPointers() {
    if (!av_frame_) return;
    
    // 初始化数据指针和行字节数（最多4个平面，覆盖绝大多数格式）
    int num_planes = av_pix_fmt_count_planes((AVPixelFormat)av_frame_->format);
    data_.resize(num_planes);
    linesize_.resize(num_planes);
    for (int i = 0; i < num_planes; ++i) {
        data_[i] = av_frame_->data[i];
        linesize_[i] = av_frame_->linesize[i];
    }
}

void MediaFrame::releaseResources() {
    if (av_frame_) {
        av_frame_free(&av_frame_); // 调用FFmpeg的释放函数（安全释放）
        av_frame_ = nullptr;
    }
}

void MediaFrame::setTimeBase(int num, int den) {
    if (den <= 0) {
        throw std::invalid_argument("setTimeBase failed: denominator must be positive");
    }
    time_base_num_ = num;
    time_base_den_ = den;
    
    // 如果已设置PTS，同步更新毫秒值
    if (pts_ != -1) {
        setPts(pts_);
    }
}

void MediaFrame::setPts(int64_t pts) {
    pts_ = pts;
    // 转换PTS为毫秒（使用FFmpeg的时间基准转换函数）
    AVRational src_tb = {time_base_num_, time_base_den_}; // 源时间基准（流的time_base）
    AVRational dst_tb = {1, 1000};                        // 目标时间基准（1/1000秒 = 毫秒）
    pts_ms_ = av_rescale_q(pts_, src_tb, dst_tb);
}
