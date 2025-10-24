//
//  data_struct.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 19/10/2025.
//

#ifndef DATA_STRUCT_H
#define DATA_STRUCT_H

#include <opencv2/opencv.hpp>
#include "ai_infer/ai_infer.h"

//解码线程->推理线程：传递 BGR
struct FrameData {
    uint8_t* data;     //内存由帧池管理
    int width;         // 宽度
    int height;        // 高度
    double pts;        // 时间戳（跨平台通用）
};

//推理县城->渲染线程：帧+推理结果
struct ResultData {
    cv::Mat rgb_mat;
    int frame_index;
    AIResult ai_result;
};

#endif /* DATA_STRUCT_H */
