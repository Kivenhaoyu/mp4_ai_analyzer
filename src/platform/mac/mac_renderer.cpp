//
//  cv_renderer.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 18/10/2025.
//

#include <stdio.h>
#include "mac_renderer.h"

MACFrameRenderer::MACFrameRenderer():is_quit_(false), is_pause_(false) {
}

MACFrameRenderer::~MACFrameRenderer() {
    //销毁 window
    cv::destroyWindow(window_name_);
}

bool MACFrameRenderer::init(const std::string& title,int width, int height) {
    window_name_ = title;
    // 创建窗口大小（可以调整）
    cv::namedWindow(window_name_,cv::WINDOW_AUTOSIZE);
//    预缓存文字参数（固定格式，只计算一次）
    initTextCache();
}

//渲染 BGR 数据
bool MACFrameRenderer::render(const FrameData& frame, const std::string& text){
    if (!frame.data || frame.width <= 0 || frame.height <= 0) {
        std::cerr << "渲染失败:无效的 BGR 数据或者尺寸" << std::endl;
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // 线程安全
    
    // 将 BGR 数据转换成 openCV 的 Mat (RGB24格式)
    frame_ = cv::Mat(frame.height, frame.width, CV_8UC3, const_cast<uint8_t*>(frame.data));
    // 添加文字，如果有
    if (!text.empty()) {
        int font = cv::FONT_HERSHEY_PLAIN;
        double font_scale = 0.7;
        int thickness = 2;
        cv::Scalar text_color(255,255,255); //白色文字
        cv::Scalar bg_color(0,0,255); //红色背景

        //文字尺寸
        cv::Size text_size = cv::getTextSize(text, font, font_scale, thickness, nullptr);
        //文字背景位置（左上角10，10）
        cv::Rect br_rect(10,10,text_size.width+10,text_size.height+10);
        cv::rectangle(frame_, br_rect, bg_color, -1); //填充背景
        //绘制文字
        cv::putText(frame_, text, cv::Point(15,15+text_size.height), font, font_scale, text_color,thickness);
    }
    
    //显示画面
    cv::imshow(window_name_, frame_);
    
    handleEvents();

    return  true;
    
}

//检查是否需要推出（按q键退出）
bool MACFrameRenderer::should_quit() {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_quit_;
}

//暂停/继续
void MACFrameRenderer::toggle_pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_pause_ = !is_pause_;
}
