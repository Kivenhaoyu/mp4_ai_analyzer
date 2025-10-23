//
//  cv_renderer.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 18/10/2025.
//

#ifndef CV_RENDERER_H
#define CV_RENDERER_H

#include <string>
#include <mutex>
#include <opencv2/opencv.hpp>

// 渲染累，接收 BGR 数据并显示数据，支持叠加文字
class CVFrameRenderer {
public:
    CVFrameRenderer(const std::string& window_name, int init_width = 640, int init_height = 480);
    
    ~CVFrameRenderer();
    
    //渲染 BGR 数据
    bool render(const uint8_t* rgb_data, int width, int height, const std::string& text = "");
    
    //检查是否需要推出（按q键退出）
    bool shouldQuit() const;
    
    //暂停/继续
    void togglePause();
    
private:
    std::string window_name_;       //窗口名称
    cv::Mat frame_;                 //缓存的 BRG 帧
    mutable std::mutex mutex_;      //线程安全锁
    bool is_quit_;                  //退出标志
    bool is_pause_;                 //暂停标志
    
};


#endif /* CV_RENDERER_H */
