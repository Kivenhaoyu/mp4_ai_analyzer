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
}

class FFmpegDecoder {
public:
    FFmpegDecoder(){};
    bool open(const std::string& file_path);
    void close();
    bool getFrame(AVFrame *frame);
    int getVideoWidth();
    int getVideoHeight();
    std::string getErrorMsg();
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
};

#endif /* DECODER_H_ */
