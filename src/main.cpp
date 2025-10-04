//
//  main.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//

#include <stdio.h>
#include "decoder/decoder.h"
using namespace std;

int main() {
    
//    avformat_network_init();
    FFmpegDecoder *decoder = new FFmpegDecoder();
    bool openRet = decoder->open("/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/data/test.mp4");
    if (!openRet) {
        cout << decoder->getErrorMsg() << endl;
    }else {
        cout << "文件打开成功" << endl;
    }
    decoder->close();
    delete decoder;
    
//    avformat_network_deinit();

    return 0;
}
