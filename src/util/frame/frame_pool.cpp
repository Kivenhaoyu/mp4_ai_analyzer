//
//  frame_pool.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 13/10/2025.
//

#include <stdio.h>
#include "frame_pool.h"
#include <stdexcept>

MediaFramePool::MediaFramePool(size_t max_cache_per_key):max_cache_size_(max_cache_per_key) {

}

//从池子里取
MediaFramePtr MediaFramePool::acquire(int width, int height, PixelFormat fmt) {
    if (width <= 0 || height <= 0 || fmt == PixelFormat::UNKNOWN) {
        throw std::invalid_argument("MediaFramePool;:acquire width/height/fmt invalid");
    }
    FrameKey key{width,height,fmt};
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end() && !it->second.empty()) {
        MediaFramePtr media_frame = it->second.front();
        it->second.pop();
        reset_frame(media_frame.get());
        return media_frame;
    }
    return MediaFrame::createEmpty(width, height, fmt);
}

//释放
void MediaFramePool::release(MediaFramePtr frame) {
    if (!frame) {
        return;
    }
    
    FrameKey key{frame->width(),frame->height(),frame->format()};
    std::lock_guard<std::mutex> lock(mutex_);
    auto& queue = cache_[key];
    if (queue.size() < max_cache_size_) {
        queue.push(std::move(frame));
    }
}

//清理指定的帧池
void MediaFramePool::clear(int width, int height, PixelFormat fmt) {
    FrameKey key{width,height,fmt};
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(key);
}

//清理所有
void MediaFramePool::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

void MediaFramePool::reset_frame(MediaFrame *frame) {
    if(!frame) return;
    frame->setPts(-1);
    frame->setTimeBase(1, 1);
}

