//
//  frame_pool.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 13/10/2025.
//

#ifndef FRAME_POOL_H
#define FRAME_POOL_H

#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <mutex>
#include "../../common/media_frame.h"

struct FrameKey {
    int width;
    int height;
    PixelFormat format;
    
    bool operator == (const FrameKey& other) const {
        return width == other.width && height == other.height && format == other.format;
    }
};

namespace std {
template<> struct hash<FrameKey> {
    size_t operator()(const FrameKey& key) const {
        size_t hash_val = 17;
        hash_val = hash_val * 31 + hash<int>()(key.width);
        hash_val = hash_val * 31 + hash<int>()(key.height);
        hash_val = hash_val * 31 + hash<int>()(static_cast<int>(key.format));
        return hash_val;
        }
    };
}

/**
 * 标准帧池：复用 MediaFrame，减少 Frame 频繁分配/释放的开销
 * 线程安全，支持多线程并发 acquire/release
 */
class MediaFramePool {

public:
    explicit MediaFramePool(size_t max_cache_per_key = 30);
    
    //不允许拷贝构造
    MediaFramePool(const MediaFramePool&) = delete;
    //不允许移动构造
    MediaFramePool& operator=(const  MediaFramePool&) = delete;
    
    MediaFramePool(MediaFramePool&&) noexcept = default;
    MediaFramePool& operator=(MediaFramePool&&) noexcept = default;
    
    //获取帧，若没有，则内部新建
    MediaFramePtr acquire(int width, int height, PixelFormat fmt);
    
    //释放
    void release(MediaFramePtr frame);
    
    //清理指定的帧池
    void clear(int width, int height, PixelFormat fmt);
    
    //清理所有
    void clearAll();
    
    
    int setMaxCacheSize(int max_size) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_cache_size_ = max_size;
        
    }
    
private:
    // 重置帧状态（复用前清理临时数据）
    void reset_frame(MediaFrame* frame);
    
    mutable std::mutex mutex_; //锁
    size_t max_cache_size_ = 30; //最大缓存数
    std::unordered_map<FrameKey, std::queue<MediaFramePtr>> cache_;

};


#endif /* FRAME_POOL_H */
