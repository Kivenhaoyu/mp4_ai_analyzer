//
//  frame_converter.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#include <stdio.h>
#include "frame_converter.h"

FrameConverter::FrameConverter() {
    
}

FrameConverter::~FrameConverter() {
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    if (mid_frame_) {
        av_frame_free(&mid_frame_);
        mid_frame_ = nullptr;
    }
}

/**
 * YUV→BGR格式转换 + 裁剪/黑边/拉伸 + 缩放
 * @param src_yuv 输入YUV帧（支持YUV420P、UYVY422）
 * @param dst_bgr 输出BGR帧（AV_PIX_FMT_BGR24）
 * @param dst_w 目标宽度
 * @param dst_h 目标高度
 * @param mode 缩放模式（STRETCH/KEEP_BLACK/CROP）
 * @return 成功返回true
 */
bool FrameConverter::convertCropResizeYuvToBgr(const AVFrame* yuv_frame, AVFrame* bgr_frame,
                                              int dst_w, int dst_h, ResizeMode mode) {
    // 1. 入参校验
    if (!yuv_frame || !bgr_frame) {
        LOG_ERROR("处理失败：输入/输出帧为空");
        return false;
    }
    AVPixelFormat src_fmt = static_cast<AVPixelFormat>(yuv_frame->format);
    if (src_fmt != AV_PIX_FMT_YUV420P && src_fmt != AV_PIX_FMT_UYVY422) {
        LOG_ERROR("暂不支持的YUV格式（仅支持YUV420P和UYVY422）");
        return false;
    }
    if (dst_w <= 0 || dst_h <= 0) {
        LOG_ERROR("目标尺寸无效（宽=" + std::to_string(dst_w) + ", 高=" + std::to_string(dst_h) + "）");
        return false;
    }
    
    int mid_w = dst_w, mid_h = dst_h;
    int crop_w = yuv_frame->width, crop_h = yuv_frame->height;
    int crop_x = 0, crop_y = 0;
    calcCropResizeParams(yuv_frame->width, yuv_frame->height, dst_w, dst_h, mode, crop_x, crop_y, crop_w, crop_h, mid_w, mid_h);
    
    
    // 关键：检查并分配 bgr_frame 的缓冲区（若未分配）
    if (bgr_frame->width != dst_w ||
        bgr_frame->height != dst_h ||
        bgr_frame->format != AV_PIX_FMT_BGR24) {
        // 尺寸/格式不匹配，先释放旧缓冲区
        av_frame_unref(bgr_frame);
        // 设置新的宽高和格式
        bgr_frame->width = dst_w;//yuv_frame->width;
        bgr_frame->height =dst_h;//yuv_frame->height;
        bgr_frame->format = AV_PIX_FMT_BGR24;
        // 分配缓冲区（32字节对齐，兼容多数硬件）
        if (av_frame_get_buffer(bgr_frame, 32) < 0) {
            LOG_ERROR("YUV422p 转 BGR 失败：RGB帧缓冲区分配失败");
            return false;
        }
    }
    
    // 准备数据源，为 crop 做数据准备
    uint8_t* src_data[4]={nullptr};
    int src_linesize[4]={0};
    if (src_fmt == AV_PIX_FMT_YUV420P){
        // yyyyuuvv
        src_data[0] = const_cast<uint8_t*>(yuv_frame->data[0]) + crop_y * yuv_frame->linesize[0] + crop_x;
        src_data[1] = const_cast<uint8_t*>(yuv_frame->data[1]) + crop_y * yuv_frame->linesize[1]/2 + crop_x/2;
        src_data[2] = const_cast<uint8_t*>(yuv_frame->data[2]) + crop_y * yuv_frame->linesize[2]/2 + crop_x/2;
        src_linesize[0] = yuv_frame->linesize[0];
        src_linesize[1] = yuv_frame->linesize[1];
        src_linesize[2] = yuv_frame->linesize[2];
    }else if (src_fmt == AV_PIX_FMT_UYVY422) {
        //bgrbgr
        src_data[0] = const_cast<uint8_t*>(yuv_frame->data[0]) + crop_y * yuv_frame->linesize[0] + crop_x*2;
        src_linesize[0] = yuv_frame->linesize[0];
    }
    
    // 创建缩放上下文
    int current_dst_w = (mode == ResizeMode::KEEP_BLACK) ? mid_w : dst_w;
    int current_dst_h = (mode == ResizeMode::KEEP_BLACK) ? mid_h : dst_h;
    
    if (!initSwsContext(crop_w, crop_h, src_fmt,
                        current_dst_w, current_dst_h, AV_PIX_FMT_BGR24)) {
        return false;
    }
    
    // 执行缩放（同时转换格式）
    bool success = false;
    if (mode != ResizeMode::KEEP_BLACK) {
        int ret = sws_scale(sws_ctx_, src_data, src_linesize, 0, crop_h, bgr_frame->data, bgr_frame->linesize);
        success = ret == dst_h;
    } else {
        if (!mid_frame_) {
            mid_frame_ = av_frame_alloc();
        }
        // 检查中间帧尺寸是否匹配，不匹配则重新分配
        if (mid_frame_->width != mid_w || mid_frame_->height != mid_h || mid_frame_->format != AV_PIX_FMT_BGR24) {
            av_frame_unref(mid_frame_);  // 释放旧缓冲区
            mid_frame_->width = mid_w;
            mid_frame_->height = mid_h;
            mid_frame_->format = AV_PIX_FMT_BGR24;
            if (av_frame_get_buffer(mid_frame_, 32) < 0) {
                LOG_ERROR("中间帧缓冲区分配失败");
                return false;
            }
        }
        //先缩放到中间帧
        int ret = sws_scale(sws_ctx_, src_data, src_linesize, 0, crop_h, mid_frame_->data, mid_frame_->linesize);
        if (ret != mid_h) {
            LOG_ERROR("中间帧缩放失败");
            return false;
        }
        
        memset(bgr_frame->data[0], 0, bgr_frame->linesize[0]*bgr_frame->height);
        // 居中复制中间帧到目标帧（计算偏移：水平/垂直居中）
        const int x_off = (dst_w - mid_w) / 2;  // 水平黑边宽度
        const int y_off = (dst_h - mid_h) / 2;  // 垂直黑边高度
        for (int y = 0; y < mid_h; ++y) {
            uint8_t* src_row = mid_frame_->data[0]+mid_frame_->linesize[0]*y;
            uint8_t* dst_row = bgr_frame->data[0]+bgr_frame->linesize[0] * (y+y_off) + x_off*3;
            memcpy(dst_row, src_row, mid_w * 3);
        }
        success = true;
    }
    if (!success) {
        LOG_ERROR("缩放失败（实际处理行数不匹配）");
        return false;
    }
    
    return true;
}

void FrameConverter::calcCropResizeParams(int src_w, int src_h, int dst_w, int dst_h, ResizeMode mode, int& crop_x, int& crop_y, int& crop_w, int& crop_h, int& mid_w, int& mid_h) {
    
    const float src_ratio = ((float)src_w)/src_h;
    const float dst_ratio = ((float)dst_w)/dst_h;
    
    switch (mode) {
        case ResizeMode::STRETCH:
        {//默认拉伸
            break;
        }
        case ResizeMode::KEEP_BLACK:
        {   //保持比例，空白填黑色
            if (src_ratio > dst_ratio) {
                mid_h = static_cast<int>(dst_w / src_ratio);
            }else {
                mid_w = static_cast<int>(dst_h * src_ratio);
            }
            // 强制修正为正数（至少2像素，确保偶数）
            mid_w = std::max(2, mid_w);
            mid_h = std::max(2, mid_h);
            mid_w = (mid_w % 2 == 0) ? mid_w : mid_w - 1;  // 确保偶数
            mid_h = (mid_h % 2 == 0) ? mid_h : mid_h - 1;
            break;
        }
        case ResizeMode::CROP:
        {   //裁剪模式
            if (src_ratio > dst_ratio) {
                crop_w = crop_h * dst_ratio;
            }else {
                crop_h = crop_w / dst_ratio;
            }
            crop_x = std::max(0,(src_w - crop_w)/2);
            crop_y = std::max(0,(src_h - crop_h)/2);
            crop_w = std::min(crop_w, src_w-crop_x);
            crop_h = std::min(crop_h, src_h-crop_y);
            crop_w = (crop_w % 2 == 0) ? crop_w : crop_w - 1;
            crop_h = (crop_h % 2 == 0) ? crop_h : crop_h - 1;
            break;
        }
            
        default:
            break;
    }
}

bool FrameConverter::initSwsContext(int src_w, int src_h, AVPixelFormat src_fmt, int dst_w, int dst_h, AVPixelFormat dst_fmt){
    if (sws_ctx_ && src_w == last_src_w_ && src_h == last_src_h_ && src_fmt == last_src_fmt_ && dst_w == last_dst_w_ && dst_h == last_dst_h_) {
        return true;
    }
    
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
    }
    // 创建新上下文
    sws_ctx_ = sws_getContext(
                                    src_w, src_h, src_fmt,
                                    dst_w, dst_h, dst_fmt,
                                    SWS_BILINEAR, nullptr, nullptr, nullptr
                                    );
    if (!sws_ctx_) {
        LOG_ERROR("创建缩放上下文失败");
        return false;
    }
    // 更新缓存参数
    last_src_w_ = src_w;
    last_src_h_ = src_h;
    last_src_fmt_ = src_fmt;
    last_dst_w_ = dst_w;
    last_dst_h_ = dst_h;    
    return true;
}
