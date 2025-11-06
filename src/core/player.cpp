//
//  player.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 3/11/2025.
//

#include <stdio.h>
#include "player.h"
#include "../common/log/log.h"


Player::Player() {
}

Player::~Player() {
    close();
}

void Player::openFile(const std::string file_path) {
    if (file_path.empty()) {
        error_msg_ = "file path is empty!!!";
        
        return;
    }
}

void Player::close() {
    if (contex_.is_valid) {
        if (contex_.format_ctx) {
            avformat_free_context(contex_.format_ctx);
        }
    }
}

//bool VideoDecoder::openWithDevice(const std::string& camera_path, bool device_type) {
//    if(format_ctx_) {
//        close();
//    }
//    if (!packet_) {
//        error_msg_ = "AVPacket内存分配失败";
//        return false;
//    }
//    if (camera_path.empty()) {
//        error_msg_ = "路径为空";
//        return false;
//    }
//    avdevice_register_all();  // 注册所有输入输出设备（avfoundation依赖这个）
//    
//    const AVInputFormat * input_fmt = nullptr;
//    // 新增：设置摄像头参数（分辨率、帧率）
//    AVDictionary* device_options = nullptr;
//    if (device_type) {
//#ifdef  __linux__
//        input_fmt = av_find_input_format("v4l2"); //Linux 摄像头
//#elif __APPLE__
//        input_fmt = av_find_input_format("avfoundation"); // macOS摄像头（Xcode环境用这个）
//#elif _WIN32
//        input_fmt = av_find_input_format("dshow");  // Windows摄像头
//#else
//        error_msg_ = "不支持的系统";
//        return false;
//#endif
//        if (!input_fmt) {
//            error_msg_ = "找不到摄像头输入格式";
//            return false;
//        }
//        
//        av_dict_set(&device_options, "video_size", "1280x720", 0);  // 分辨率（必须是支持的，如640x480或1280x720）
//        av_dict_set(&device_options, "framerate", "30", 0);         // 帧率（30，匹配支持的30.000030）
//        av_dict_set(&device_options, "pixel_format", "uyvy422", 0); // 新增像素格式参数
//    }
//    
//    // 打开文件/摄像头（设备模式需传入input_fmt）
//    int ret = avformat_open_input(&format_ctx_, camera_path.c_str(), input_fmt, &device_options);
//    av_dict_free(&device_options);
//    if (ret != 0) {
//        error_msg_ = saveError(ret, "打开摄像头失败：");
//        return false;
//    }
//    
//    ret = avformat_find_stream_info(format_ctx_, nullptr);
//    if(ret < 0) {
//        error_msg_ = saveError(ret, "获取流信息失败");
//        close();
//        return false;
//    }
//    video_stream_index_ = -1;
//    for (unsigned int i = 0; i< format_ctx_->nb_streams; i++) {
//        if(format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
//            video_stream_index_ = i;
//            break;
//        }
//    }
//    
//    if (video_stream_index_ == -1) {
//        error_msg_ = "未找到视频流";
//        close();
//        return false;
//    }
//    
//    // 根据视频流参数，查找对应的解码器
//    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
//    codec_ = avcodec_find_decoder(codec_par->codec_id);
//    if (!codec_) {
//        error_msg_ = "找不到对应的解码器（codec_id）:" + std::to_string(codec_par->codec_id);
//        close();
//        return false;
//    }
//    
//    codec_ctx_ = avcodec_alloc_context3(codec_);
//    if (!codec_ctx_) {
//        error_msg_ = "分配解码器上下文失败";
//        close();
//        return false;
//    }
//    ret = avcodec_parameters_to_context(codec_ctx_, codec_par);
//    if (ret < 0) {
//        error_msg_ = "复制流参数到解码器上下文失败：" + std::string(av_err2str(ret));
//        close();
//        return false;
//    }
//    // 打开解码器前添加（需要FFmpeg编译时支持对应硬件加速）
//    AVDictionary* codec_options = nullptr; // 用于存储解码器选项
//    AVBufferRef* hw_device_ctx = nullptr; // 硬件设备上下文
//#ifdef __APPLE__
//    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
//    if (ret < 0) {
//        error_msg_ = "创建VideoToolbox硬件设备上下文失败：" +std::string(av_err2str(ret));
//        // 不终止，继续尝试软件解码
//    } else {
//        // 将硬件设备上下文关联到解码器
//        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
//        av_dict_set(&codec_options, "hwaccel", "videotoolbox", 0);
//    }
//#elif _WIN32
//    // Windows类似逻辑（d3d11va）
//    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
//    if (ret >= 0) {
//        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
//        av_dict_set(&options, "hwaccel", "d3d11va", 0);
//    }
//#elif __linux__
//    // Linux VAAPI逻辑
//    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
//    if (ret >= 0) {
//        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
//        av_dict_set(&codec_options, "hwaccel", "vaapi", 0);
//    }
//#endif
//    // 打开解码器前设置线程数（建议为 CPU 核心数，避免过度并行）
//    codec_ctx_->thread_count = std::thread::hardware_concurrency();
//    codec_ctx_->thread_type = FF_THREAD_FRAME; // 按帧并行（适合视频）
//    // 打开解码器（最终准备就绪，可以开始解码）
//    codec_ctx_->thread_count = 1; // 实时场景单线程更稳定
//    // 打开解码器（最终准备就绪，可以开始解码）
//    ret = avcodec_open2(codec_ctx_, codec_, &codec_options);
//    av_dict_free(&codec_options);
//    // 4. 释放硬件设备上下文（已通过av_buffer_ref关联到codec_ctx_，此处仅释放原始引用）
//    if (hw_device_ctx) {
//        av_buffer_unref(&hw_device_ctx);
//    }
//    
//    // 5. 硬件加速失败时重试软件解码
//    if (ret < 0) {
//        error_msg_ = "硬件加速解码失败，尝试软件解码：" + std::string(av_err2str(ret));
//        // 重置解码器上下文的硬件相关参数（避免影响软件解码）
//        if (codec_ctx_->hw_device_ctx) {
//            av_buffer_unref(&codec_ctx_->hw_device_ctx);
//            codec_ctx_->hw_device_ctx = nullptr;
//        }
//        // 重试软件解码
//        ret = avcodec_open2(codec_ctx_, codec_, nullptr);
//        if (ret < 0) {
//            error_msg_ = "软件解码也失败：" + std::string(av_err2str(ret));
//            close();
//            return false;
//        }
//    }
//    
//    // 6. 验证硬件加速是否生效（更准确的判断方式）
//    if (codec_ctx_->hwaccel || codec_ctx_->hw_device_ctx) {
//        std::cout << "硬件加速解码已启用（类型：" << (codec_ctx_->hwaccel ? codec_ctx_->hwaccel->name : "videotoolbox") << "）" << std::endl;
//    } else {
//        std::cout << "使用软件解码" << std::endl;
//    }
//    
//    return true;
//}
//

