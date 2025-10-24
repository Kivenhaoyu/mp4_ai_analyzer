//
//  frame_pool.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 13/10/2025.
//

#include <stdio.h>
#include "frame_pool.h"

AVFramePool::AVFramePool(int size, int width, int height, AVPixelFormat pix_fmt):width_(width),height_(height),pix_fmt_(pix_fmt) {
    for(int i = 0 ; i < size; i ++) {
        AVFrame *frame = av_frame_alloc();
        frame->width = width;
        frame->height = height;
        frame->format = pix_fmt;
        if(av_frame_get_buffer(frame, 32) < 0) {
            av_frame_free(&frame);
            throw std::runtime_error("帧缓冲区分配失败");
        }
        pool_.push_back(std::unique_ptr<AVFrame>(frame));
    }
}

//从池子里取
std::unique_ptr<AVFrame> AVFramePool::getFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(pool_.empty()) {
        AVFrame * frame = av_frame_alloc();
        frame->width = width_;
        frame->height = height_;
        frame->format = pix_fmt_;
        // 分配缓冲区时检查,分配内存
        av_frame_get_buffer(frame, 32);
        return std::unique_ptr<AVFrame>(frame);
    }
    
    auto frame = std::move(pool_.back());
    pool_.pop_back();
    // 确保可写
    // 若缓冲区已可写：直接返回成功；
    // 若缓冲区只读：自动复制一份新的可写缓冲区，让 frame->data 指向新内存。
    av_frame_make_writable(frame.get());
    return frame;
}

//返回给池子
void AVFramePool::returnFrame(std::unique_ptr<AVFrame> frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    av_frame_unref(frame.get());
    pool_.push_back(std::move(frame));
}
