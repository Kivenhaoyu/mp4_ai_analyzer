//
//  decoder.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//

#ifndef DECODER_H_
#define DECODER_H_

#include <stdio.h>
#include <iostream>

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}

class FFmpegDecoder {
public:
    FFmpegDecoder();
    bool openWithLocalFile(const std::string& file_path);
    bool openWithDevice(const std::string& camera_path, bool device_type);
    void close();
    bool getFrame(AVFrame *frame);
    int getVideoWidth();
    int getVideoHeight();
    std::string getErrorMsg();
    std::string getVideoCodecName();
    // YUV 转 RGB
    bool convertYuvToRgb(const AVFrame* yuv_frame, AVFrame* rgb_frame);
    
    bool converUYUV422ToRgb(const AVFrame* yuv_frame, AVFrame*rgb_frame);
    
    bool saveRGBFrameToJPG(const AVFrame* rgb_frame, const std::string &save_path);
    
    // RGB帧缩放至目标尺寸（默认224×224，AI模型常用）
    bool resizeRGBFrame(const AVFrame* src_rgb, AVFrame* dst_rgb, int dst_w = 224, int dst_h = 224);
    
    // RGB帧像素归一化：[0,255]→[(x/255 - mean)/std]（默认mean=0.5, std=0.5→范围[-1,1]）
    bool normalizeRGBFrame(const AVFrame* rgb_frame, float* output_buf,
                           const std::vector<float>& mean,
                           const std::vector<float>& std);

private:
    // 格式上下文（存储视频文件整体信息：路径，流数量，时长等）
    AVFormatContext *format_ctx_ = nullptr;
    // 解码器上下文（存储解码器参数：宽高、像素格式、解码器指针等）
    AVCodecContext *codec_ctx_ = nullptr;
    // 视频流索引（标记哪个是视频流，文件可能包含视频流/音频流/字幕流等）
    int video_stream_index_ = -1;
    // 解码器(对应视频的解码器，如 H。264 解码器)
    const AVCodec *codec_ = nullptr;
    // 错误信息（记录打开/解码过程中的错误，方便调试）
    std::string error_msg_;
    // 数据包
    std::unique_ptr<AVPacket,void(*)(AVPacket *)> packet_;
    
    SwsContext* sws_ctx_ = nullptr;
        
    const std::string saveError(int err_code, const std::string& prefix);
};

#endif /* DECODER_H_ */
