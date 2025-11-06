//
//  frame_converter.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#ifndef frame_converter_h
#define frame_converter_h

#include <stdio.h>
#include <iostream>
#include <stdlib.h>

extern "C" {
#include <libswscale/swscale.h>
}

#include "../../common/log/log.h"

enum class ResizeMode {
    STRETCH,    // 拉伸：直接缩放到目标尺寸（可能变形）
    KEEP_BLACK, // 保留黑边：保持比例，不足部分填黑边（无变形）
    CROP        // 裁剪适配：先裁剪到目标比例，再缩放（无变形、无黑边）
};

class FrameConverter {
    
public:
    FrameConverter();
    ~FrameConverter();
    /**
     * YUV→BGR格式转换 + 裁剪/黑边/拉伸 + 缩放
     * @param src_yuv 输入YUV帧（支持YUV420P、UYVY422）
     * @param dst_bgr 输出BGR帧（AV_PIX_FMT_BGR24）
     * @param dst_w 目标宽度（如224）
     * @param dst_h 目标高度（如224）
     * @param mode 缩放模式（STRETCH/KEEP_BLACK/CROP）
     * @return 成功返回true
     */
    bool convertCropResizeYuvToBgr(const AVFrame* yuv_frame, AVFrame* bgr_frame, int dst_w, int dst_h, ResizeMode mode = ResizeMode::KEEP_BLACK);
    
private:
    SwsContext* sws_ctx_ = nullptr;
    AVFrame* mid_frame_ = nullptr;
    
    // 缓存上一次的转换参数
    int last_src_w_ = -1, last_src_h_ = -1;
    AVPixelFormat last_src_fmt_ = AV_PIX_FMT_NONE;
    int last_dst_w_ = -1, last_dst_h_ = -1;
    
    
    // 计算缩放/裁剪参数
    void calcCropResizeParams(int src_w, int src_h, int dst_w, int dst_h, ResizeMode mode, int& crop_x, int& crop_y, int& crop_w, int& crop_h, int& mid_w, int& mid_h);
    bool initSwsContext(int src_w, int src_h, AVPixelFormat src_fmt, int dst_w, int dst_h, AVPixelFormat dst_fmt);
};


#endif /* frame_converter_h */
