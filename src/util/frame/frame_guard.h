//
//  frame_guard.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 15/10/2025.
//

#ifndef FRAME_GUARD_H
#define FRAME_GUARD_H

#include <memory>
#include "frame_pool.h"

template <typename PoolType>
class MediaFrameGuard {
public:
    explicit MediaFrameGuard(PoolType& pool, MediaFramePtr frame):pool_(pool),frame_(std::move(frame)){}
    
    // 禁止复制（避免同一帧被多次归还）
    MediaFrameGuard(const MediaFrameGuard&) = delete;
    MediaFrameGuard& operator=(const MediaFrameGuard&) = delete;
    
    // 允许移动（支持所有权转移）
    MediaFrameGuard(MediaFrameGuard &&) noexcept = default;
    MediaFrameGuard& operator = (MediaFrameGuard &&) noexcept = default;
    
    ~MediaFrameGuard() {
        if (frame_) {
            pool_.returnFrame(std::move(frame_));
        }
    }
    
    // 获取智能指针（用于访问帧的成员）
    MediaFramePtr get() {
        return frame_;
    }
    
    MediaFramePtr& get() const {
        return frame_;
    }
    
    // 重载 -> 运算符
    MediaFramePtr* operator->() {
        return &frame_;
    }
    
    const MediaFramePtr* operator->() const {
        return &frame_;
    }
    
private:
    PoolType& pool_; // 帧池引用（生命周期由外部保证）
    MediaFramePtr frame_;  // 管理的帧
};

#endif /* FRAME_GUARD_H */
