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
class FrameGuard {
public:
    explicit FrameGuard(PoolType& pool):pool_(pool),frame_(pool.getFrame()){}
    
    // 禁止复制（避免同一帧被多次归还）
    FrameGuard(const FrameGuard&) = delete;
    FrameGuard& operator=(const FrameGuard&) = delete;
    
    // 允许移动（支持所有权转移）
    FrameGuard(FrameGuard &&) noexcept = default;
    FrameGuard& operator = (FrameGuard &&) noexcept = default;
    
    ~FrameGuard() {
        if (frame_) {
            pool_.returnFrame(std::move(frame_));
        }
    }
    
    // 获取智能指针（用于访问帧的成员）
    std::unique_ptr<AVFrame>& get() {
        return frame_;
    }
    
    std::unique_ptr<AVFrame>& get() const {
        return frame_;
    }
    
    // 重载 -> 运算符
    std::unique_ptr<AVFrame>* operator->() {
        return &frame_;
    }
    
    const std::unique_ptr<AVFrame>* operator->() const {
        return &frame_;
    }
    
private:
    PoolType& pool_; // 帧池引用（生命周期由外部保证）
    std::unique_ptr<AVFrame> frame_;  // 管理的帧
};

#endif /* FRAME_GUARD_H */
