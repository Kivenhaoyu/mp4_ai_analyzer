//
//  main.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//

#include <stdio.h>
#include <memory>
#include "decoder/decoder.h"
using namespace std;

int main() {
    
//    avformat_network_init();
    FFmpegDecoder *decoder = new FFmpegDecoder();
    bool ret = decoder->open("/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/data/test.mp4");
    if (!ret) {
        cout << decoder->getErrorMsg() << endl;
    }else {
        cout << "文件打开成功:" << decoder->getVideoWidth() << ":" << decoder->getVideoHeight() << "编码格式：" << decoder->getVideoCodecName() << endl;
    }
    
    auto frame_deleter = [](AVFrame *f) {
        if (f != nullptr) {
            av_frame_free(&f);
        }
    };
    
    unique_ptr<AVFrame, decltype(frame_deleter) > frame(av_frame_alloc(),frame_deleter);
    ret = decoder->getFrame(frame.get());
    if (!ret) {
        cout << "获取首帧失败" << endl;
        cout << decoder->getErrorMsg() << endl;
    }else {
        cout << "获取首帧成功" << endl;
    }
    
    decoder->close();
    delete decoder;
    
//    avformat_network_deinit();

    return 0;
}
