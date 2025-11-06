//
//  decoder.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//


#include "video_decoder.h"
#include <thread>
#include <opencv2/opencv.hpp>
#include "../common/log/log.h"

VideoDecoder::~VideoDecoder() {
    close();  // 确保 close() 中释放 mid_frame_
}

VideoDecoder::VideoDecoder(PlayerContext& ctx) : packet_(av_packet_alloc(),[](AVPacket *pkt){
    av_packet_free(&pkt);
}),ctx_(ctx){
    if (!packet_) {
        error_msg_ = "AVPacket 内存分配失败";
    }
}

bool VideoDecoder::openVideoDecoder(const std::string& file_path) {
    if (!packet_) {
        error_msg_ = "AVPacket 内存分配失败";
        return false;
    }
    // 打开文件路径
    if(file_path.empty()){
        error_msg_ = "文件路径为空！！！";
        return false;
    }
    int ret = avformat_open_input(&ctx_.format_ctx, file_path.c_str(), nullptr, nullptr);
    if (ret != 0) {
        error_msg_ = "打开文件失败："+std::string(av_err2str(ret));
        return false;
    }
    // 获取流信息（必须调用，否则无法找到视频流）
    ret = avformat_find_stream_info(ctx_.format_ctx, nullptr);
    if (ret < 0) {
        error_msg_ = "获取流信息失效：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    
    // 查找视频流索引（遍历所有流，找到类型为 AVMEDIA_TYPE_VIDEO 的流）
    for (unsigned int i = 0; i < ctx_.format_ctx->nb_streams; i++) {
        if (ctx_.format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }
    
    if (video_stream_index_ == -1) {
        // 没找到视频流
        error_msg_ = "没有视频流";
        close();
        return false;
    }
    
    // 根据视频流参数，查找对应的解码器
    AVCodecParameters* codec_par = ctx_.format_ctx->streams[video_stream_index_]->codecpar;
    codec_ = avcodec_find_decoder(codec_par->codec_id);
    if (!codec_) {
        error_msg_ = "找不到对应的解码器（codec_id）:" + std::to_string(codec_par->codec_id);
        close();
        return false;
    }
    
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        error_msg_ = "分配解码器上下文失败";
        close();
        return false;
    }
    ret = avcodec_parameters_to_context(codec_ctx_, codec_par);
    if (ret < 0) {
        error_msg_ = "复制流参数到解码器上下文失败：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    
    // 打开解码器前添加（需要FFmpeg编译时支持对应硬件加速）
    AVDictionary* codec_options = nullptr; // 用于存储解码器选项
    AVBufferRef* hw_device_ctx = nullptr; // 硬件设备上下文
#ifdef __APPLE__
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
    if (ret < 0) {
        error_msg_ = "创建VideoToolbox硬件设备上下文失败：" +std::string(av_err2str(ret));
        // 不终止，继续尝试软件解码
    } else {
        // 将硬件设备上下文关联到解码器
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&codec_options, "hwaccel", "videotoolbox", 0);
    }
#elif _WIN32
    // Windows类似逻辑（d3d11va）
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
    if (ret >= 0) {
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&options, "hwaccel", "d3d11va", 0);
    }
#elif __linux__
    // Linux VAAPI逻辑
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
    if (ret >= 0) {
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&codec_options, "hwaccel", "vaapi", 0);
    }
#endif
    // 打开解码器前设置线程数（建议为 CPU 核心数，避免过度并行）
    codec_ctx_->thread_count = std::thread::hardware_concurrency();
    codec_ctx_->thread_type = FF_THREAD_FRAME; // 按帧并行（适合视频）
    
    // 打开解码器（最终准备就绪，可以开始解码）
    ret = avcodec_open2(codec_ctx_, codec_, &codec_options);
    av_dict_free(&codec_options);
    // 4. 释放硬件设备上下文（已通过av_buffer_ref关联到codec_ctx_，此处仅释放原始引用）
    if (hw_device_ctx) {
        av_buffer_unref(&hw_device_ctx);
    }
    
    // 5. 硬件加速失败时重试软件解码
    if (ret < 0) {
        error_msg_ = "硬件加速解码失败，尝试软件解码：" + std::string(av_err2str(ret));
        // 重置解码器上下文的硬件相关参数（避免影响软件解码）
        if (codec_ctx_->hw_device_ctx) {
            av_buffer_unref(&codec_ctx_->hw_device_ctx);
            codec_ctx_->hw_device_ctx = nullptr;
        }
        // 重试软件解码
        ret = avcodec_open2(codec_ctx_, codec_, nullptr);
        if (ret < 0) {
            error_msg_ = "软件解码也失败：" + std::string(av_err2str(ret));
            close();
            return false;
        }
    }
    
    // 6. 验证硬件加速是否生效（更准确的判断方式）
    if (codec_ctx_->hwaccel || codec_ctx_->hw_device_ctx) {
        std::cout << "硬件加速解码已启用（类型：" << (codec_ctx_->hwaccel ? codec_ctx_->hwaccel->name : "videotoolbox") << "）" << std::endl;
    } else {
        std::cout << "使用软件解码" << std::endl;
    }
    return true;
}

void VideoDecoder::close() {
    if (codec_ctx_) {
        if (codec_ctx_->hw_device_ctx) {
            av_buffer_unref(&codec_ctx_->hw_device_ctx);
        }
        avcodec_close(codec_ctx_);
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }

    video_stream_index_ = -1;
    codec_ = nullptr;
    error_msg_.clear();    
}

MediaFramePtr VideoDecoder::getFrame() {
    if (!ctx_.is_valid || !ctx_.format_ctx || !codec_ctx_ || video_stream_index_ < 0 || !packet_) {
        error_msg_ = "解码器初始化参数无效";
        return nullptr;
    }
    AVFrame* frame = av_frame_alloc();
    
    while(true) {
        // 读取一个数据包（压缩数据）
        int ret = av_read_frame(ctx_.format_ctx, packet_.get());
        if (ret < 0) {
            // 读取完毕或者出错：尝试 flush 解码器中剩余的帧
            avcodec_send_packet(codec_ctx_, nullptr);
        }else {
            // 只处理视频流的数据包
            if (packet_->stream_index != video_stream_index_) {
                av_packet_unref(packet_.get());
                continue;
            }
            
            // 将数据包发送到解码器
            int send_ret = avcodec_send_packet(codec_ctx_, packet_.get());
            if (send_ret < 0) {
                error_msg_ = "发送数据包到解码器失败";
                LOG_ERROR(error_msg_);
                av_packet_unref(packet_.get());
                return nullptr;
            }
            av_packet_unref(packet_.get());
        }
        ret = avcodec_receive_frame(codec_ctx_, frame);
        MediaFramePtr media_frame;
        if (ret == 0) {
            try {
                media_frame = MediaFrame::createFromAVFrame(frame);
                return media_frame;
            } catch (const std::exception& e) {
                av_frame_free(&frame);
                return nullptr;
            }
            
        } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // EAGAIN: 需要更多数据；EOF: 解码器已无数据
            if (ret == AVERROR_EOF) {
                av_frame_free(&frame);
                return nullptr;
            }
            continue;
        }else {
            // 其他错误
            error_msg_ = "解码失败，错误码: " + std::to_string(ret);
            LOG_ERROR(error_msg_);
            av_frame_free(&frame);
            return nullptr;
        }
    }
}

void VideoDecoder::getVideoSize(int &width, int &height) {
    if (!ctx_.is_valid || !ctx_.format_ctx) {
        error_msg_ = "获取视频宽度失败：格式上下文未初始化或已关闭";
        return;
    }
    if (video_stream_index_ < 0) {
        error_msg_ = "获取视频宽度失败：未找到视频流";
        return;
    }
    AVCodecParameters* codec_par = ctx_.format_ctx->streams[video_stream_index_]->codecpar;
    width = codec_par->width;
    height = codec_par->height;
}

std::string VideoDecoder::getVideoCodecName() {
    if (!ctx_.is_valid || !ctx_.format_ctx) {
        error_msg_ = "获取视频编码格式失败：格式上下文未初始化或已关闭";
        return error_msg_;
    }
    if (video_stream_index_ < 0) {
        error_msg_ = "获取视频编码格式失败：未找到视频流";
        return error_msg_;
    }
    AVCodecParameters* codec_par = ctx_.format_ctx->streams[video_stream_index_]->codecpar;
    return avcodec_get_name(codec_par->codec_id);
}

#pragma mark -- Error --

std::string VideoDecoder::getErrorMsg() {
    return error_msg_;
}

const std::string VideoDecoder::saveError(int err_code, const std::string& prefix) {
    char err_buf[1024] = {0};
    av_strerror(err_code, err_buf, sizeof(err_buf));  // 用 FFmpeg 接口获取错误描述
    return prefix + std::string(err_buf);
}

