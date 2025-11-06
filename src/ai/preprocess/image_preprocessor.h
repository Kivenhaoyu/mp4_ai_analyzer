//
//  image_preprocessor.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#ifndef IMAGE_PREPROCESSOR_H
#define IMAGE_PREPROCESSOR_H

#include <vector>

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}

class ImagePreprocessor {
public:
    // BGR帧归一化：[0,255] → [(x/255 - mean)/std]
    static bool normalizeBGRFrame(const AVFrame* bgr_frame, float* output_buf,
                                  const std::vector<float>& mean,
                                  const std::vector<float>& std);
};

#endif /* IMAGE_PREPROCESSOR_H */
