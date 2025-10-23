//
//  cv_renderer.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 18/10/2025.
//

#include <stdio.h>
#include "cv_renderer.h"

CVFrameRenderer::CVFrameRenderer(const std::string& window_name, int init_width, int init_height):window_name_(window_name), is_quit_(false), is_pause_(false) {
    // 创建窗口大小（可以调整）
    cv::namedWindow(window_name_,cv::WINDOW_NORMAL);
    // 根据初始化大小调整
    cv::resizeWindow(window_name_, init_width, init_height);
}

CVFrameRenderer::~CVFrameRenderer() {
    //销毁 window
    cv::destroyWindow(window_name_);
}

//渲染 BGR 数据
bool CVFrameRenderer::render(const uint8_t* rgb_data, int width, int height, const std::string& text){
    if (!rgb_data || width <= 0 || height <= 0) {
        std::cerr << "渲染失败:无效的 BGR 数据或者尺寸" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_); // 线程安全
    
    // 将 BGR 数据转换成 openCV 的 Mat (RGB24格式)
    cv::Mat rgb_mat(height, width, CV_8UC3, const_cast<uint8_t*>(rgb_data));
    
    // 缩小到窗口尺寸（width/2, height/2）
    cv::resize(rgb_mat, frame_, cv::Size(width/2, height/2), 0, 0, cv::INTER_LINEAR);
        
    // 添加文字，如果有
    if (!text.empty()) {
        int font = cv::FONT_HERSHEY_SIMPLEX;
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
    
    //处理键盘（q退出，空格暂停）
    int key = cv::waitKey(1);  //等待1ms，检测输入
    if (key == 'q' || key == 'Q') {
        is_quit_ = true;
    }else if (key == ' ') {
        is_pause_ = !is_pause_;
        std::cout << (is_pause_ ? "已暂停（按空格继续）" : "已继续") << std::endl;
        if(is_pause_) {
            // 暂停等待，直到再次按空格
            while (is_pause_ && !is_quit_) {
                key = cv::waitKey(100);
                if (key == ' ') {
                    is_pause_ = false;
                }else if (key == 'q') {
                    is_quit_ = true;
                }
            }
        }
    }
    return  true;
    
}

//检查是否需要推出（按q键退出）
bool CVFrameRenderer::shouldQuit() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_quit_;
}

//暂停/继续
void CVFrameRenderer::togglePause() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_pause_ = !is_pause_;
}
