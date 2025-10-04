//
//  decoder.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//

#include "decoder.h"

FFmpegDecoder::FFmpegDecoder() {
    packet_ = av_packet_alloc();
}

bool FFmpegDecoder::open(const std::string& file_path) {
    // 打开文件路径
    if(file_path.empty()){
        error_msg_ = "文件路径为空！！！";
        return false;
    }
    int ret = avformat_open_input(&format_ctx_, file_path.c_str(), nullptr, nullptr);
    if (ret != 0) {
        error_msg_ = "打开文件失败："+std::string(av_err2str(ret));
        return false;
    }
    // 获取流信息（必须调用，否则无法找到视频流）
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        error_msg_ = "获取流信息失效：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    
    // 查找视频流索引（遍历所有流，找到类型为 AVMEDIA_TYPE_VIDEO 的流）
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
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
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
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
    
    // 打开解码器（最终准备就绪，可以开始解码）
    ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        error_msg_ = "打开解码器失败：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    return true;
}

void FFmpegDecoder::close() {
    if (codec_ctx_) {
        avcodec_close(codec_ctx_);
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
    video_stream_index_ = -1;
    codec_ = nullptr;
    error_msg_.clear();
    av_packet_free(&packet_);
    
}

bool FFmpegDecoder::getFrame(AVFrame *frame) {
    if (!format_ctx_ || !codec_ctx_ || video_stream_index_ < 0 || !packet_ || !frame) {
        error_msg_ = "解码器初始化参数无效";
        return false;
    }
    while(true) {
        // 读取一个数据包（压缩数据）
        int ret = av_read_frame(format_ctx_, packet_);
        if (ret < 0) {
            // 读取完毕或者出错：尝试 flush 解码器中剩余的帧
            avcodec_send_packet(codec_ctx_, nullptr);
        }else {
            // 只处理视频流的数据包
            if (packet_->stream_index != video_stream_index_) {
                av_packet_unref(packet_);
                continue;
            }
            
            // 将数据包发送到解码器
            if (avcodec_send_packet(codec_ctx_, packet_) < 0) {
                std::cerr << "发送数据包到解码器失败" << std::endl;
                av_packet_unref(packet_);
                return false;
            }
            av_packet_unref(packet_);
        }
        ret = avcodec_receive_frame(codec_ctx_, frame);
        if (ret == 0) {
            return true;
        } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // EAGAIN: 需要更多数据；EOF: 解码器已无数据
            if (ret == AVERROR_EOF) {
                return false;
            }
            continue;
        }else {
            // 其他错误
            std::cerr << "解码失败，错误码: " << ret << std::endl;
            return false;
        }
    }
}

int FFmpegDecoder::getVideoWidth() {
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
    return codec_par->width;
}

int FFmpegDecoder::getVideoHeight() {
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
    return codec_par->height;
}

std::string FFmpegDecoder::getVideoCodecName() {
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
    return avcodec_get_name(codec_par->codec_id);
}

std::string FFmpegDecoder::getErrorMsg() {
    return error_msg_;
}
