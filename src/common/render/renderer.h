//
//  cv_renderer.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 18/10/2025.
//

#ifndef RENDERER_H
#define RENDERER_H

#include <string>
#include <mutex>
#include <opencv2/opencv.hpp>
#include "../data_structs.h"

// 渲染累，接收 BGR 数据并显示数据，支持叠加文字
class Renderer {
public:
    virtual ~Renderer() = default;
    // 初始化：传入窗口宽高、标题
    virtual bool init(const std::string& title,int width, int height) = 0;
    // 渲染一帧：输入RGB数据
    virtual bool render(const FrameData& frame, const std::string& text = "") = 0;
    // 检查是否需要退出（比如用户关闭窗口）
    virtual bool should_quit() = 0;
};


#endif /* RENDERER_H */
