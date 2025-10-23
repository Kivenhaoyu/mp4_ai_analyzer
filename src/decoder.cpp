//
//  decoder.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//


#include "decoder.h"
#include <thread>
#include <opencv2/opencv.hpp>

/**
 * 手动填充图像为黑色（替代av_image_fill_black，兼容旧版FFmpeg）
 * @param data 图像数据指针（同AVFrame.data）
 * @param linesize 每行字节数（同AVFrame.linesize）
 * @param w 图像宽度
 * @param h 图像高度
 * @param pix_fmt 像素格式
 */
void fillBlack(uint8_t* data[4], const int linesize[4], int w, int h, AVPixelFormat pix_fmt) {
    switch (pix_fmt) {
        case AV_PIX_FMT_BGR24: {
            // BGR24格式：黑色 = B=0, G=0, R=0（每个像素3字节）
            for (int y = 0; y < h; y++) {
                // 每行起始地址
                uint8_t* row = data[0] + y * linesize[0];
                // 填充一行：连续写入w个像素（每个3字节，值为0）
                memset(row, 0, w * 3);
                // 若linesize > w*3（内存对齐的填充部分），也填0（可选，不影响显示）
                if (linesize[0] > w * 3) {
                    memset(row + w * 3, 0, linesize[0] - w * 3);
                }
            }
            break;
        }
        case AV_PIX_FMT_YUV420P: {
            // YUV420P格式：黑色 = Y=0, U=128, V=128（YUV的中性值）
            // 1. 填充Y平面（亮度）
            for (int y = 0; y < h; y++) {
                uint8_t* row = data[0] + y * linesize[0];
                memset(row, 0, w); // Y=0
                if (linesize[0] > w) {
                    memset(row + w, 0, linesize[0] - w);
                }
            }
            // 2. 填充U平面（色度，宽高为Y的1/2）
            int u_w = w / 2;
            int u_h = h / 2;
            for (int y = 0; y < u_h; y++) {
                uint8_t* row = data[1] + y * linesize[1];
                memset(row, 128, u_w); // U=128
                if (linesize[1] > u_w) {
                    memset(row + u_w, 128, linesize[1] - u_w);
                }
            }
            // 3. 填充V平面（同U）
            for (int y = 0; y < u_h; y++) {
                uint8_t* row = data[2] + y * linesize[2];
                memset(row, 128, u_w); // V=128
                if (linesize[2] > u_w) {
                    memset(row + u_w, 128, linesize[2] - u_w);
                }
            }
            break;
        }
        case AV_PIX_FMT_UYVY422: {
            // UYVY422格式：黑色 = U=128, Y=0, V=128（每个像素2字节，按U-Y-V-Y排列）
            for (int y = 0; y < h; y++) {
                uint8_t* row = data[0] + y * linesize[0];
                // 逐像素填充（每4字节对应2个Y像素）
                for (int x = 0; x < w; x += 2) {
                    row[x*2 + 0] = 128; // U
                    row[x*2 + 1] = 0;   // Y0
                    row[x*2 + 2] = 128; // V
                    row[x*2 + 3] = 0;   // Y1
                }
                // 处理内存对齐的填充部分
                int total_bytes = w * 2; // 理论总字节数
                if (linesize[0] > total_bytes) {
                    memset(row + total_bytes, 0, linesize[0] - total_bytes);
                }
            }
            break;
        }
        default:
            break;
    }
}


cv::Mat cropBlackEdges(const cv::Mat& bgr_mat) {
    if (bgr_mat.empty()) return bgr_mat;
    cv::Mat gray_mat;
    cv::cvtColor(bgr_mat, gray_mat, cv::COLOR_BGR2GRAY);
    // 直接获取非黑边掩码（阈值>10的区域）
    cv::Mat mask = gray_mat > 10;
    // 找到掩码中非零区域的边界
    cv::Rect roi = cv::boundingRect(mask);
    if (roi.empty()) return bgr_mat;
    return bgr_mat(roi);
}

FFmpegDecoder::FFmpegDecoder() : packet_(av_packet_alloc(),[](AVPacket *pkt){
    av_packet_free(&pkt);
}){
    if (!packet_) {
        error_msg_ = "AVPacket 内存分配失败";
    }
}

bool FFmpegDecoder::openWithLocalFile(const std::string& file_path) {
    if (!packet_) {
        error_msg_ = "AVPacket 内存分配失败";
        return false;
    }
    // 打开文件路径
    if(file_path.empty()){
        error_msg_ = "文件路径为空！！！";
        return false;
    }
    int ret = avformat_open_input(&format_ctx_, file_path.c_str(), nullptr, nullptr);
    if (ret != 0) {
        error_msg_ = "打开文件失败："+std::string(av_err2str(ret));
        return false;
    }
    // 获取流信息（必须调用，否则无法找到视频流）
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        error_msg_ = "获取流信息失效：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    
    // 查找视频流索引（遍历所有流，找到类型为 AVMEDIA_TYPE_VIDEO 的流）
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }
    
    if (video_stream_index_ == -1) {
        // 没找到视频流
        error_msg_ = "没有视频流";
        close();
        return false;
    }
    
    // 根据视频流参数，查找对应的解码器
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
    codec_ = avcodec_find_decoder(codec_par->codec_id);
    if (!codec_) {
        error_msg_ = "找不到对应的解码器（codec_id）:" + std::to_string(codec_par->codec_id);
        close();
        return false;
    }
    
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        error_msg_ = "分配解码器上下文失败";
        close();
        return false;
    }
    ret = avcodec_parameters_to_context(codec_ctx_, codec_par);
    if (ret < 0) {
        error_msg_ = "复制流参数到解码器上下文失败：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    
    // 打开解码器前添加（需要FFmpeg编译时支持对应硬件加速）
    AVDictionary* codec_options = nullptr; // 用于存储解码器选项
    AVBufferRef* hw_device_ctx = nullptr; // 硬件设备上下文
#ifdef __APPLE__
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
    if (ret < 0) {
        error_msg_ = "创建VideoToolbox硬件设备上下文失败：" +std::string(av_err2str(ret));
        // 不终止，继续尝试软件解码
    } else {
        // 将硬件设备上下文关联到解码器
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&codec_options, "hwaccel", "videotoolbox", 0);
    }
#elif _WIN32
    // Windows类似逻辑（d3d11va）
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
    if (ret >= 0) {
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&options, "hwaccel", "d3d11va", 0);
    }
#elif __linux__
    // Linux VAAPI逻辑
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
    if (ret >= 0) {
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&codec_options, "hwaccel", "vaapi", 0);
    }
#endif
    // 打开解码器前设置线程数（建议为 CPU 核心数，避免过度并行）
    codec_ctx_->thread_count = std::thread::hardware_concurrency();
    codec_ctx_->thread_type = FF_THREAD_FRAME; // 按帧并行（适合视频）
    
    // 打开解码器（最终准备就绪，可以开始解码）
    ret = avcodec_open2(codec_ctx_, codec_, &codec_options);
    av_dict_free(&codec_options);
    // 4. 释放硬件设备上下文（已通过av_buffer_ref关联到codec_ctx_，此处仅释放原始引用）
    if (hw_device_ctx) {
        av_buffer_unref(&hw_device_ctx);
    }
    
    // 5. 硬件加速失败时重试软件解码
    if (ret < 0) {
        error_msg_ = "硬件加速解码失败，尝试软件解码：" + std::string(av_err2str(ret));
        // 重置解码器上下文的硬件相关参数（避免影响软件解码）
        if (codec_ctx_->hw_device_ctx) {
            av_buffer_unref(&codec_ctx_->hw_device_ctx);
            codec_ctx_->hw_device_ctx = nullptr;
        }
        // 重试软件解码
        ret = avcodec_open2(codec_ctx_, codec_, nullptr);
        if (ret < 0) {
            error_msg_ = "软件解码也失败：" + std::string(av_err2str(ret));
            close();
            return false;
        }
    }
    
    // 6. 验证硬件加速是否生效（更准确的判断方式）
    if (codec_ctx_->hwaccel || codec_ctx_->hw_device_ctx) {
        std::cout << "硬件加速解码已启用（类型：" << (codec_ctx_->hwaccel ? codec_ctx_->hwaccel->name : "videotoolbox") << "）" << std::endl;
    } else {
        std::cout << "使用软件解码" << std::endl;
    }
    
    // 初始化 SwsContext
    sws_ctx_ = sws_getContext(codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_YUV420P, codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
        error_msg_ = "SwsContext 初始化失败（YUV 转 BGR 上下文创建失败）";
        close();  // 初始化失败，释放已分配的资源
        return false;
    }
    return true;
}

bool FFmpegDecoder::openWithDevice(const std::string& camera_path, bool device_type) {
    if(format_ctx_) {
        close();
    }
    if (!packet_) {
        error_msg_ = "AVPacket内存分配失败";
        return false;
    }
    if (camera_path.empty()) {
        error_msg_ = "路径为空";
        return false;
    }
    avdevice_register_all();  // 注册所有输入输出设备（avfoundation依赖这个）
    
    const AVInputFormat * input_fmt = nullptr;
    // 新增：设置摄像头参数（分辨率、帧率）
    AVDictionary* device_options = nullptr;
    if (device_type) {
#ifdef  __linux__
        input_fmt = av_find_input_format("v4l2"); //Linux 摄像头
#elif __APPLE__
        input_fmt = av_find_input_format("avfoundation"); // macOS摄像头（Xcode环境用这个）
#elif _WIN32
        input_fmt = av_find_input_format("dshow");  // Windows摄像头
#else
        error_msg_ = "不支持的系统";
        return false;
#endif
        if (!input_fmt) {
            error_msg_ = "找不到摄像头输入格式";
            return false;
        }
        
        av_dict_set(&device_options, "video_size", "1280x720", 0);  // 分辨率（必须是支持的，如640x480或1280x720）
        av_dict_set(&device_options, "framerate", "30", 0);         // 帧率（30，匹配支持的30.000030）
        av_dict_set(&device_options, "pixel_format", "uyvy422", 0); // 新增像素格式参数
    }
    
    // 打开文件/摄像头（设备模式需传入input_fmt）
    int ret = avformat_open_input(&format_ctx_, camera_path.c_str(), input_fmt, &device_options);
    av_dict_free(&device_options);
    if (ret != 0) {
        error_msg_ = saveError(ret, "打开摄像头失败：");
        return false;
    }
    
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if(ret < 0) {
        error_msg_ = saveError(ret, "获取流信息失败");
        close();
        return false;
    }
    video_stream_index_ = -1;
    for (unsigned int i = 0; i< format_ctx_->nb_streams; i++) {
        if(format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }
    
    if (video_stream_index_ == -1) {
        error_msg_ = "未找到视频流";
        close();
        return false;
    }
    
    // 根据视频流参数，查找对应的解码器
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
    codec_ = avcodec_find_decoder(codec_par->codec_id);
    if (!codec_) {
        error_msg_ = "找不到对应的解码器（codec_id）:" + std::to_string(codec_par->codec_id);
        close();
        return false;
    }
    
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        error_msg_ = "分配解码器上下文失败";
        close();
        return false;
    }
    ret = avcodec_parameters_to_context(codec_ctx_, codec_par);
    if (ret < 0) {
        error_msg_ = "复制流参数到解码器上下文失败：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    // 打开解码器前添加（需要FFmpeg编译时支持对应硬件加速）
    AVDictionary* codec_options = nullptr; // 用于存储解码器选项
    AVBufferRef* hw_device_ctx = nullptr; // 硬件设备上下文
#ifdef __APPLE__
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
    if (ret < 0) {
        error_msg_ = "创建VideoToolbox硬件设备上下文失败：" +std::string(av_err2str(ret));
        // 不终止，继续尝试软件解码
    } else {
        // 将硬件设备上下文关联到解码器
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&codec_options, "hwaccel", "videotoolbox", 0);
    }
#elif _WIN32
    // Windows类似逻辑（d3d11va）
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
    if (ret >= 0) {
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&options, "hwaccel", "d3d11va", 0);
    }
#elif __linux__
    // Linux VAAPI逻辑
    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
    if (ret >= 0) {
        codec_ctx_->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_dict_set(&codec_options, "hwaccel", "vaapi", 0);
    }
#endif
    // 打开解码器前设置线程数（建议为 CPU 核心数，避免过度并行）
    codec_ctx_->thread_count = std::thread::hardware_concurrency();
    codec_ctx_->thread_type = FF_THREAD_FRAME; // 按帧并行（适合视频）
    // 打开解码器（最终准备就绪，可以开始解码）
    codec_ctx_->thread_count = 1; // 实时场景单线程更稳定
    // 打开解码器（最终准备就绪，可以开始解码）
    ret = avcodec_open2(codec_ctx_, codec_, &codec_options);
    av_dict_free(&codec_options);
    // 4. 释放硬件设备上下文（已通过av_buffer_ref关联到codec_ctx_，此处仅释放原始引用）
    if (hw_device_ctx) {
        av_buffer_unref(&hw_device_ctx);
    }
    
    // 5. 硬件加速失败时重试软件解码
    if (ret < 0) {
        error_msg_ = "硬件加速解码失败，尝试软件解码：" + std::string(av_err2str(ret));
        // 重置解码器上下文的硬件相关参数（避免影响软件解码）
        if (codec_ctx_->hw_device_ctx) {
            av_buffer_unref(&codec_ctx_->hw_device_ctx);
            codec_ctx_->hw_device_ctx = nullptr;
        }
        // 重试软件解码
        ret = avcodec_open2(codec_ctx_, codec_, nullptr);
        if (ret < 0) {
            error_msg_ = "软件解码也失败：" + std::string(av_err2str(ret));
            close();
            return false;
        }
    }
    
    // 6. 验证硬件加速是否生效（更准确的判断方式）
    if (codec_ctx_->hwaccel || codec_ctx_->hw_device_ctx) {
        std::cout << "硬件加速解码已启用（类型：" << (codec_ctx_->hwaccel ? codec_ctx_->hwaccel->name : "videotoolbox") << "）" << std::endl;
    } else {
        std::cout << "使用软件解码" << std::endl;
    }
    
    // 初始化 SwsContext
    sws_ctx_ = sws_getContext(codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_UYVY422, codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
        error_msg_ = "SwsContext 初始化失败（YUV 转 BGR 上下文创建失败）";
        close();  // 初始化失败，释放已分配的资源
        return false;
    }
    
    return true;
}


void FFmpegDecoder::close() {
    
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    
    if (codec_ctx_) {
        if (codec_ctx_->hw_device_ctx) {
            av_buffer_unref(&codec_ctx_->hw_device_ctx);
        }
        avcodec_close(codec_ctx_);
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
    video_stream_index_ = -1;
    codec_ = nullptr;
    error_msg_.clear();
    
}

bool FFmpegDecoder::getFrame(AVFrame *frame) {
    static int frame_count = 0;
    if (!format_ctx_ || !codec_ctx_ || video_stream_index_ < 0 || !packet_ || !frame) {
        error_msg_ = "解码器初始化参数无效";
        return false;
    }
    
    av_frame_unref(frame);
    
    if (frame_count % 100 == 0 && frame_count > 0) {
        avcodec_flush_buffers(codec_ctx_);
        frame_count = 0;
    }
    frame_count++;
    
    while(true) {
        av_packet_unref(packet_.get());
        // 读取一个数据包（压缩数据）
        int ret = av_read_frame(format_ctx_, packet_.get());
        if (ret < 0) {
            // 读取完毕或者出错：尝试 flush 解码器中剩余的帧
            avcodec_send_packet(codec_ctx_, nullptr);
        }else {
            // 只处理视频流的数据包
            if (packet_->stream_index != video_stream_index_) {
                av_packet_unref(packet_.get());
                continue;
            }
            
            // 将数据包发送到解码器
            int send_ret = avcodec_send_packet(codec_ctx_, packet_.get());
            if (send_ret < 0) {
                error_msg_ = "发送数据包到解码器失败";
                std::cerr << error_msg_ << std::endl;
                av_packet_unref(packet_.get());
                return false;
            }
            av_packet_unref(packet_.get());
        }
        ret = avcodec_receive_frame(codec_ctx_, frame);
        if (ret == 0) {
            // 打印像素格式：YUV420P 对应 AV_PIX_FMT_YUV420P（值为 0）
            //            std::cout << "[解码帧信息] 像素格式：" << frame->format
            //                    << "（需为 YUV420P：" << AV_PIX_FMT_YUV420P << "）" << std::endl;
            return true;
        } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // EAGAIN: 需要更多数据；EOF: 解码器已无数据
            if (ret == AVERROR_EOF) {
                return false;
            }
            continue;
        }else {
            // 其他错误
            error_msg_ = "解码失败，错误码: " + std::to_string(ret);
            std::cerr << error_msg_ << std::endl;
            return false;
        }
    }
}

int FFmpegDecoder::getVideoWidth() {
    if (!format_ctx_) {
        error_msg_ = "获取视频宽度失败：格式上下文未初始化或已关闭";
        return -1;
    }
    if (video_stream_index_ < 0) {
        error_msg_ = "获取视频宽度失败：未找到视频流";
        return -1;
    }
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
    return codec_par->width;
}

int FFmpegDecoder::getVideoHeight() {
    if (!format_ctx_) {
        error_msg_ = "获取视频高度失败：格式上下文未初始化或已关闭";
        return -1;
    }
    if (video_stream_index_ < 0) {
        error_msg_ = "获取视频高度失败：未找到视频流";
        return -1;
    }
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
    return codec_par->height;
}

std::string FFmpegDecoder::getVideoCodecName() {
    if (!format_ctx_) {
        error_msg_ = "获取视频编码格式失败：格式上下文未初始化或已关闭";
        return error_msg_;
    }
    if (video_stream_index_ < 0) {
        error_msg_ = "获取视频编码格式失败：未找到视频流";
        return error_msg_;
    }
    AVCodecParameters* codec_par = format_ctx_->streams[video_stream_index_]->codecpar;
    return avcodec_get_name(codec_par->codec_id);
}

/**
 * 一步完成：YUV→BGR格式转换 + 裁剪/黑边/拉伸 + 缩放
 * @param src_yuv 输入YUV帧（支持YUV420P、UYVY422）
 * @param dst_bgr 输出BGR帧（AV_PIX_FMT_BGR24）
 * @param dst_w 目标宽度（如224）
 * @param dst_h 目标高度（如224）
 * @param mode 缩放模式（STRETCH/KEEP_BLACK/CROP）
 * @return 成功返回true
 */
bool FFmpegDecoder::convertCropResizeYuvToBgr(const AVFrame* src_yuv, AVFrame* bgr_frame,
                                              int dst_w, int dst_h, ResizeMode mode) {
    // 1. 入参校验
    if (!src_yuv || !bgr_frame) {
        error_msg_ = "处理失败：输入/输出帧为空";
        return false;
    }
    AVPixelFormat src_fmt = static_cast<AVPixelFormat>(src_yuv->format);
    if (src_fmt != AV_PIX_FMT_YUV420P && src_fmt != AV_PIX_FMT_UYVY422) {
        error_msg_ = "暂不支持的YUV格式（仅支持YUV420P和UYVY422）";
        return false;
    }
    if (dst_w <= 0 || dst_h <= 0) {
        error_msg_ = "目标尺寸无效（宽=" + std::to_string(dst_w) + ", 高=" + std::to_string(dst_h) + "）";
        return false;
    }
    
    // 关键：检查并分配 bgr_frame 的缓冲区（若未分配）
    if (bgr_frame->width != dst_w ||
        bgr_frame->height != dst_h ||
        bgr_frame->format != AV_PIX_FMT_BGR24) {
        // 尺寸/格式不匹配，先释放旧缓冲区
        av_frame_unref(bgr_frame);
        // 设置新的宽高和格式
        bgr_frame->width = dst_w;//yuv_frame->width;
        bgr_frame->height =dst_h;//yuv_frame->height;
        bgr_frame->format = AV_PIX_FMT_BGR24;
        // 分配缓冲区（32字节对齐，兼容多数硬件）
        if (av_frame_get_buffer(bgr_frame, 32) < 0) {
            error_msg_ = "YUV422p 转 BGR 失败：RGB帧缓冲区分配失败";
            return false;
        }
    }
    
    // 创建缩放上下文
    SwsContext* sws_ctx = sws_getContext(
                                         src_yuv->width, src_yuv->height, static_cast<AVPixelFormat>(src_yuv->format),  // 源参数：宽、高、格式
        dst_w, dst_h, AV_PIX_FMT_BGR24,    // 目标参数：宽、高、格式
        SWS_BILINEAR,                      // 缩放算法（双线性插值，平衡速度和质量）
        nullptr, nullptr, nullptr          // 滤波器参数（默认nullptr即可）
    );

    if (!sws_ctx) {
        std::cerr << "创建缩放上下文失败" << std::endl;
        return false;
    }
    
    // 执行缩放（同时转换格式）
    int ret = sws_scale(
        sws_ctx,                  // 缩放上下文
        src_yuv->data,            // 源数据指针（YUV420P的3个平面）
        src_yuv->linesize,        // 源每行字节数（linesize，含对齐填充）
        0,                        // 从源图像第0行开始处理
                        src_yuv->height,                    // 处理的源图像行数（整个源图像高度）
        bgr_frame->data,            // 目标数据指针（BGR24的单平面）
                        bgr_frame->linesize         // 目标每行字节数（linesize）
    );

    // 检查缩放结果：返回值应为目标高度（dst_h），否则缩放失败
    if (ret != dst_h) {
        std::cerr << "缩放失败（实际处理行数：" << ret << "，预期：" << dst_h << "）" << std::endl;
        sws_freeContext(sws_ctx);
        return false;
    }

    return true;
}

bool FFmpegDecoder::saveBGRFrameToJPG(const AVFrame* bgr_frame, const std::string &save_path) {
    if (!bgr_frame || save_path.empty()){
        error_msg_ = "保存JPG失败：RGB帧为空";
        return false;
    }

    if (bgr_frame->format != AV_PIX_FMT_BGR24) {
        error_msg_ = "保存JPG失败：输入不是BGR24格式（实际格式：" + std::to_string(bgr_frame->format) + "）";
        return false;
    }
    if (bgr_frame->width <= 0 || bgr_frame->height <= 0) {
        error_msg_ = "保存JPG失败：帧宽高无效";
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
        error_msg_ = "保存JPG失败：无法写入文件（路径：" + save_path + "）";
        return false;
    }

    return true;

}

bool FFmpegDecoder::normalizeBGRFrame(const AVFrame* bgr_frame, float* output_buf,
                                      const std::vector<float>& mean,
                                      const std::vector<float>& std) {
    // 入参校验（保留核心检查）
    if (!bgr_frame || !output_buf || bgr_frame->format != AV_PIX_FMT_BGR24) {
        error_msg_ = "归一化失败：输入帧无效或格式不是BGR24";
        return false;
    }
    if (std[0] == 0 || std[1] == 0 || std[2] == 0) {
        error_msg_ = "归一化失败：标准差不能为0";
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




#pragma mark -- Error --

std::string FFmpegDecoder::getErrorMsg() {
    return error_msg_;
}

const std::string FFmpegDecoder::saveError(int err_code, const std::string& prefix) {
    char err_buf[1024] = {0};
    av_strerror(err_code, err_buf, sizeof(err_buf));  // 用 FFmpeg 接口获取错误描述
    return prefix + std::string(err_buf);
}

