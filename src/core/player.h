//
//  player.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 3/11/2025.
//

#ifndef player_h
#define player_h

#include <stdio.h>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

struct PlayerContext {
    // 格式上下文（存储视频文件整体信息：路径，流数量，时长等）
    AVFormatContext *format_ctx = nullptr;
    bool is_valid; // 标志 true 表示有效，false 表示空
    
    PlayerContext() {
        is_valid = true;
    }
};

class Player {
public:
    Player();
    ~Player();
    void openFile(const std::string filePath);
    void close();
    
private:
    PlayerContext contex_; //上下文信息
    std::string error_msg_; //错误信息
};


#endif /* player_h */
