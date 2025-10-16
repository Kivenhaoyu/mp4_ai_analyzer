//
//  frame_pool.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 13/10/2025.
//

#ifndef FRAME_POOL_H
#define FRAME_POOL_H

#include <vector>
#include <memory>
#include <mutex>
extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}

class AVFramePool {
private:
    std::vector<std::unique_ptr<AVFrame>> pool_;
    std::mutex mutex_; // 多线程安全
    int width_;
    int height_;
    AVPixelFormat pix_fmt_;
public:
    AVFramePool(int size, int width, int height, AVPixelFormat pix_fmt);
    std::unique_ptr<AVFrame> getFrame();
    void returnFrame(std::unique_ptr<AVFrame> frame);
};


#endif /* FRAME_POOL_H */
