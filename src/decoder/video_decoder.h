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

#include "../common/media_frame.h"
#include "../util/frame/frame_pool.h"
#include "../core/player.h"

class VideoDecoder {
public:
    VideoDecoder(PlayerContext& ctx);
    ~VideoDecoder();
    bool openVideoDecoder(const std::string& file_path);
    void close();
    MediaFramePtr getFrame();
    void getVideoSize(int& width, int& height);
    std::string getErrorMsg();
    std::string getVideoCodecName();
    
private:
    PlayerContext& ctx_;
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
    
    MediaFramePool frame_pool_; //解码帧池
    
    const std::string saveError(int err_code, const std::string& prefix);
};

#endif /* DECODER_H_ */
