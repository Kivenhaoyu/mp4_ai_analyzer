//
//  decoder.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//

#include "decoder.h"

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
    
}
bool FFmpegDecoder::getFrame(AVFrame *frame) {
    
}
int FFmpegDecoder::getVideoWidth() {
    
}
int FFmpegDecoder::getVideoHeight() {
    
}
std::string FFmpegDecoder::getErrorMsg() {
    return error_msg_;
}
