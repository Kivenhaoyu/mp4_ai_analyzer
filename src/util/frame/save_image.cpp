//
//  save_image.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#include <stdio.h>
#include "../../common/log/log.h"
#include "save_image.h"
#include <opencv2/opencv.hpp>

bool SaveImage::saveBGRFrameToJPG(const AVFrame* bgr_frame, const std::string &save_path) {
    if (!bgr_frame || save_path.empty()){
        LOG_ERROR("保存JPG失败：RGB帧为空");
        return false;
    }
    
    if (bgr_frame->format != AV_PIX_FMT_BGR24) {
        LOG_ERROR("保存JPG失败：输入不是BGR24格式（实际格式：" + std::to_string(bgr_frame->format) + "）");
        return false;
    }
    if (bgr_frame->width <= 0 || bgr_frame->height <= 0) {
        LOG_ERROR("保存JPG失败：帧宽高无效");
        return false;
    }
    cv::Mat rgb_mat(
                    bgr_frame->height,          // 图像高度
                    bgr_frame->width,           // 图像宽度
                    CV_8UC3,                    // 数据类型：8位无符号，3通道
                    bgr_frame->data[0],         // 像素数据起始地址
                    bgr_frame->linesize[0]);    // 每行数据的字节数（对齐用）
    
    cv::Mat bgr_mat;
    cv::cvtColor(rgb_mat, bgr_mat, cv::COLOR_RGB2BGR);
    
    // 4. 保存为JPG（质量可选，0-100，越高质量越好）
    std::vector<int> jpg_params = {cv::IMWRITE_JPEG_QUALITY, 90};
    if (!cv::imwrite(save_path, bgr_mat, jpg_params)) {
        LOG_ERROR("保存JPG失败：无法写入文件（路径：" + save_path + "）");
        return false;
    }
    
    return true;
    
}
