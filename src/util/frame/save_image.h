//
//  save_image.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#ifndef SAVE_IMAGE_H
#define SAVE_IMAGE_H
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string>

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}

class SaveImage {
public:
    static bool saveBGRFrameToJPG(const AVFrame* bgr_frame, const std::string &save_path);
};


#endif /* SAVE_IMAGE_H */
