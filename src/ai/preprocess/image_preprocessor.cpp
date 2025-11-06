//
//  image_preprocessor.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#include <stdio.h>
#include "image_preprocessor.h"
#include "../../common/log/log.h"


bool ImagePreprocessor::normalizeBGRFrame(const AVFrame* bgr_frame, float* output_buf,
                                      const std::vector<float>& mean,
                                      const std::vector<float>& std) {
    // 入参校验（保留核心检查）
    if (!bgr_frame || !output_buf || bgr_frame->format != AV_PIX_FMT_BGR24) {
        LOG_ERROR("归一化失败：输入帧无效或格式不是BGR24");
        return false;
    }
    if (std[0] == 0 || std[1] == 0 || std[2] == 0) {
        LOG_ERROR("归一化失败：标准差不能为0");
        return false;
    }
    
    const int frame_w = bgr_frame->width;
    const int frame_h = bgr_frame->height;
    const int channel_size = frame_w * frame_h; // 单通道像素数
    
    // 1. 预计算常数倒数（除法转乘法，提升速度）
    const float inv_255 = 1.0f / 255.0f;
    const float inv_std[3] = {1.0f / std[0], 1.0f / std[1], 1.0f / std[2]};
    const float mean_vals[3] = {mean[0], mean[1], mean[2]};
    
    // 2. 按行遍历（缓存友好：连续内存访问 NHWC格式：RRRGGGBBB....）
    for (int h = 0; h < frame_h; ++h) {
        // 行数据地址
        const uint8_t* row_data = bgr_frame->data[0] + h * bgr_frame->linesize[0];
        // 当前行在单通道中的起始索引
        const int row_base = h * frame_w;
        
        // 3. 循环展开（处理4个像素/次，减少循环次数）
        int w = 0;
        for (; w < frame_w - 3; w += 4) {
            // 一次性读取4个像素的BGR数据（共12字节）
            const uint8_t* pixel = row_data + w * 3;
            
            // 计算4个像素的B通道（并行处理）
            output_buf[row_base + w]     = (pixel[0] * inv_255 - mean_vals[0]) * inv_std[0];
            output_buf[row_base + w + 1] = (pixel[3] * inv_255 - mean_vals[0]) * inv_std[0];
            output_buf[row_base + w + 2] = (pixel[6] * inv_255 - mean_vals[0]) * inv_std[0];
            output_buf[row_base + w + 3] = (pixel[9] * inv_255 - mean_vals[0]) * inv_std[0];
            
            // 计算4个像素的G通道
            output_buf[channel_size + row_base + w]     = (pixel[1] * inv_255 - mean_vals[1]) * inv_std[1];
            output_buf[channel_size + row_base + w + 1] = (pixel[4] * inv_255 - mean_vals[1]) * inv_std[1];
            output_buf[channel_size + row_base + w + 2] = (pixel[7] * inv_255 - mean_vals[1]) * inv_std[1];
            output_buf[channel_size + row_base + w + 3] = (pixel[10] * inv_255 - mean_vals[1]) * inv_std[1];
            
            // 计算4个像素的R通道
            output_buf[2 * channel_size + row_base + w]     = (pixel[2] * inv_255 - mean_vals[2]) * inv_std[2];
            output_buf[2 * channel_size + row_base + w + 1] = (pixel[5] * inv_255 - mean_vals[2]) * inv_std[2];
            output_buf[2 * channel_size + row_base + w + 2] = (pixel[8] * inv_255 - mean_vals[2]) * inv_std[2];
            output_buf[2 * channel_size + row_base + w + 3] = (pixel[11] * inv_255 - mean_vals[2]) * inv_std[2];
        }
        
        // 处理剩余像素（不足4个的部分）
        for (; w < frame_w; ++w) {
            const uint8_t* pixel = row_data + w * 3;
            const int idx = row_base + w;
            output_buf[idx] = (pixel[0] * inv_255 - mean_vals[0]) * inv_std[0];
            output_buf[channel_size + idx] = (pixel[1] * inv_255 - mean_vals[1]) * inv_std[1];
            output_buf[2 * channel_size + idx] = (pixel[2] * inv_255 - mean_vals[2]) * inv_std[2];
        }
    }
    
    return true;
}
