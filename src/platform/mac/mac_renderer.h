//
//  cv_renderer.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 18/10/2025.
//

#ifndef CV_RENDERER_H
#define CV_RENDERER_H
#pragma once
#include <string>
#include <mutex>
#include <opencv2/opencv.hpp>

#include "../../common/render/renderer.h"
#include "../../common/platform.h"

#ifdef PLATFORM_MAC // 仅Mac编译

// 渲染累，接收 BGR 数据并显示数据，支持叠加文字
class MACFrameRenderer  : public Renderer {
public:
    MACFrameRenderer();
    ~MACFrameRenderer();
    
    bool init(const std::string& title,int width, int height) override;
    
    //渲染 BGR 数据
    bool render(const FrameData& frame, const std::string& text) override;
    
    //检查是否需要推出（按q键退出）
    bool should_quit() override;
    
    //暂停/继续
    void toggle_pause();
    
private:
    std::string window_name_;       //窗口名称
    cv::Mat frame_;                 //缓存的 BRG 帧
    mutable std::mutex mutex_;      //线程安全锁
    bool is_quit_;                  //退出标志
    bool is_pause_;                 //暂停标志
    // 预缓存的文字参数（避免重复计算）
    int font_;
    double font_scale_;
    int thickness_;
    cv::Scalar text_color_;
    cv::Size text_size_;
    
    // 预初始化文字参数（只执行一次）
        void initTextCache() {
            font_ = cv::FONT_HERSHEY_PLAIN;
            font_scale_ = 0.8;  // 小字体减少绘制开销
            thickness_ = 1;     // 薄边框加速渲染
            text_color_ = cv::Scalar(255, 255, 255);  // 白色文字（无需背景）

            // 预计算最长可能文字的尺寸（避免每次调用getTextSize）
            std::string max_text = "类别：xxx | 置信度：100.00 | 耗时：100.00ms";
            text_size_ = cv::getTextSize(max_text, font_, font_scale_, thickness_, nullptr);
        }

        // 非阻塞处理按键事件（单次检查，无循环）
        void handleEvents() {
            int key = cv::waitKey(1);  // 1ms超时，非阻塞
            if (key == 'q' || key == 'Q') {
                is_quit_ = true;
            } else if (key == ' ') {
                is_pause_ = !is_pause_;
            }
        }
    
};

#endif // PLATFORM_MAC
#endif /* CV_RENDERER_H */
