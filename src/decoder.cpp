//
//  decoder.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//


#include "decoder.h"
#include <opencv2/opencv.hpp>

cv::Mat cropBlackEdges(const cv::Mat& bgr_mat) {
    if (bgr_mat.empty()) return bgr_mat;
    
    // 1. 转换为灰度图，便于检测黑边（黑边的灰度值接近0）
    cv::Mat gray_mat;
    cv::cvtColor(bgr_mat, gray_mat, cv::COLOR_BGR2GRAY);
    
    // 2. 阈值化：将接近黑色（<10）的区域视为黑边
    cv::Mat thresh_mat;
    cv::threshold(gray_mat, thresh_mat, 10, 255, cv::THRESH_BINARY); // 非黑边区域为255（白）
    
    // 3. 找到非黑边区域的边界
    int top = 0, bottom = thresh_mat.rows - 1;
    int left = 0, right = thresh_mat.cols - 1;
    
    // 找顶部边界（第一个非黑边行）
    while (top <= bottom && cv::countNonZero(thresh_mat.row(top)) == 0) top++;
    // 找底部边界（最后一个非黑边行）
    while (bottom >= top && cv::countNonZero(thresh_mat.row(bottom)) == 0) bottom--;
    // 找左侧边界（第一个非黑边列）
    while (left <= right && cv::countNonZero(thresh_mat.col(left)) == 0) left++;
    // 找右侧边界（最后一个非黑边列）
    while (right >= left && cv::countNonZero(thresh_mat.col(right)) == 0) right--;
    
    // 4. 裁剪：如果所有区域都是黑边，则返回原图
    if (top > bottom || left > right) {
        return bgr_mat;
    }
    return bgr_mat(cv::Rect(left, top, right - left + 1, bottom - top + 1));
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
    
    // 打开解码器（最终准备就绪，可以开始解码）
    ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        error_msg_ = "打开解码器失败：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    
    // 初始化 SwsContext
    sws_ctx_ = sws_getContext(codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_YUV420P, codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
        error_msg_ = "SwsContext 初始化失败（YUV 转 RGB 上下文创建失败）";
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
    AVDictionary* options = nullptr;
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
        
        av_dict_set(&options, "video_size", "1280x720", 0);  // 分辨率（必须是支持的，如640x480或1280x720）
        av_dict_set(&options, "framerate", "30", 0);         // 帧率（30，匹配支持的30.000030）
        av_dict_set(&options, "pixel_format", "uyvy422", 0); // 新增像素格式参数
    }
    
    // 打开文件/摄像头（设备模式需传入input_fmt）
    int ret = avformat_open_input(&format_ctx_, camera_path.c_str(), input_fmt, &options);
    av_dict_free(&options);
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
    
    // 打开解码器（最终准备就绪，可以开始解码）
    codec_ctx_->thread_count = 1; // 实时场景单线程更稳定
    ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        error_msg_ = "打开解码器失败：" + std::string(av_err2str(ret));
        close();
        return false;
    }
    
    // 初始化 SwsContext
    sws_ctx_ = sws_getContext(codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_UYVY422, codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
        error_msg_ = "SwsContext 初始化失败（YUV 转 RGB 上下文创建失败）";
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

bool FFmpegDecoder::convertYuvToRgb(const AVFrame* yuv_frame, AVFrame* rgb_frame) {
    // 1. 检查参数有效性（避免空指针/无效上下文）
    if (!sws_ctx_ || !yuv_frame || !rgb_frame) {
        error_msg_ = "YUV 转 RGB 失败：参数无效（上下文/帧为空）";
        return false;
    }
    // 2. 检查输入格式是否为 YUV420P（双重确认，避免异常情况）
    if (yuv_frame->format != AV_PIX_FMT_YUV420P) {
        error_msg_ = "YUV 转 RGB 失败：输入格式不是 YUV420P（实际格式：" + std::to_string(yuv_frame->format) + "）";
        return false;
    }
    
    // 关键：检查并分配 rgb_frame 的缓冲区（若未分配）
    if (rgb_frame->width != yuv_frame->width ||
        rgb_frame->height != yuv_frame->height ||
        rgb_frame->format != AV_PIX_FMT_RGB24) {
        // 尺寸/格式不匹配，先释放旧缓冲区
        av_frame_unref(rgb_frame);
        // 设置新的宽高和格式
        rgb_frame->width = yuv_frame->width;
        rgb_frame->height = yuv_frame->height;
        rgb_frame->format = AV_PIX_FMT_RGB24;
        // 分配缓冲区（32字节对齐，兼容多数硬件）
        if (av_frame_get_buffer(rgb_frame, 32) < 0) {
            error_msg_ = "YUV422p 转 RGB 失败：RGB帧缓冲区分配失败";
            return false;
        }
    }
    // 3. 执行格式转换（FFmpeg 核心接口 sws_scale）
    int converted_lines = sws_scale(
                                    sws_ctx_,                  // 格式转换上下文
                                    yuv_frame->data,           // 输入 YUV 数据（Y/U/V 三个平面）
                                    yuv_frame->linesize,       // 输入 YUV 行对齐（避免图像拉伸）
                                    0,                         // 从第 0 行开始转换
                                    yuv_frame->height,         // 转换的行数（整个帧的高度）
                                    rgb_frame->data,           // 输出 RGB 数据（单平面，RGB 按字节排列）
                                    rgb_frame->linesize        // 输出 RGB 行对齐
                                    );
    // 4. 检查转换结果（converted_lines 应等于帧高度，否则转换不完整）
    if (converted_lines != yuv_frame->height) {
        error_msg_ = "YUV 转 RGB 失败：转换行数不完整（实际：" + std::to_string(converted_lines) + "，预期：" + std::to_string(yuv_frame->height) + "）";
        return false;
    }
    // 转换成功
    return true;
}

bool FFmpegDecoder::converUYUV422ToRgb(const AVFrame* yuv_frame, AVFrame*rgb_frame) {
    // 1. 检查参数有效性（避免空指针/无效上下文）
    if (!sws_ctx_ || !yuv_frame || !rgb_frame) {
        error_msg_ = "UYUV422 转 RGB 失败：参数无效（上下文/帧为空）";
        return false;
    }
    // 2. 检查输入格式是否为 UYUV422（双重确认，避免异常情况）
    if (yuv_frame->format != AV_PIX_FMT_UYVY422) {
        error_msg_ = "UYVY422 转 RGB 失败：输入格式不是 UYVY422（实际格式：" + std::to_string(yuv_frame->format) + "）";
        return false;
    }
    
    // 关键：检查并分配 rgb_frame 的缓冲区（若未分配）
    if (rgb_frame->width != yuv_frame->width ||
        rgb_frame->height != yuv_frame->height ||
        rgb_frame->format != AV_PIX_FMT_RGB24) {
        // 尺寸/格式不匹配，先释放旧缓冲区
        av_frame_unref(rgb_frame);
        // 设置新的宽高和格式
        rgb_frame->width = yuv_frame->width;
        rgb_frame->height = yuv_frame->height;
        rgb_frame->format = AV_PIX_FMT_RGB24;
        // 分配缓冲区（32字节对齐，兼容多数硬件）
        if (av_frame_get_buffer(rgb_frame, 32) < 0) {
            error_msg_ = "UYVY422 转 RGB 失败：RGB帧缓冲区分配失败";
            return false;
        }
    }
    
    // 4. 确保缓冲区可写（避免只读内存导致的错误）
    if (av_frame_make_writable(rgb_frame) < 0) {
        error_msg_ = "UYVY422 转 RGB 失败：RGB帧不可写";
        return false;
    }
    // 3. 执行格式转换（FFmpeg 核心接口 sws_scale）
    int converted_lines = sws_scale(
                                    sws_ctx_,                  // 格式转换上下文
                                    yuv_frame->data,           // 输入 YUV 数据（Y/U/V 三个平面）
                                    yuv_frame->linesize,       // 输入 YUV 行对齐（避免图像拉伸）
                                    0,                         // 从第 0 行开始转换
                                    yuv_frame->height,         // 转换的行数（整个帧的高度）
                                    rgb_frame->data,           // 输出 RGB 数据（单平面，RGB 按字节排列）
                                    rgb_frame->linesize        // 输出 RGB 行对齐
                                    );
    // 4. 检查转换结果（converted_lines 应等于帧高度，否则转换不完整）
    if (converted_lines != yuv_frame->height) {
        error_msg_ = "UYVY422 转 RGB 失败：转换行数不完整（实际：" + std::to_string(converted_lines) + "，预期：" + std::to_string(yuv_frame->height) + "）";
        
        //销毁
        sws_freeContext(sws_ctx_);
        
        // 用当前帧的宽高重建（兼容可能的动态尺寸变化）
        sws_ctx_ = sws_getContext(
                                  yuv_frame->width, yuv_frame->height, AV_PIX_FMT_UYVY422,
                                  rgb_frame->width, rgb_frame->height, AV_PIX_FMT_RGB24,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr
                                  );
        if (!sws_ctx_) {
            error_msg_ = "重建SwsContext失败";
            return false;
        }
        
        // 重试转换
        converted_lines = sws_scale(
                                    sws_ctx_,
                                    yuv_frame->data,
                                    yuv_frame->linesize,
                                    0,
                                    yuv_frame->height,
                                    rgb_frame->data,
                                    rgb_frame->linesize
                                    );
        if (converted_lines != yuv_frame->height) {
            error_msg_ = "重试转换仍失败";
            return false;
        }
    }
    // 转换成功
    return true;
}

bool FFmpegDecoder::saveRGBFrameToJPG(const AVFrame* rgb_frame, const std::string &save_path) {
    if (!rgb_frame || save_path.empty()){
        error_msg_ = "保存JPG失败：RGB帧为空";
        return false;
    }
    
    if (rgb_frame->format != AV_PIX_FMT_RGB24) {
        error_msg_ = "保存JPG失败：输入不是RGB24格式（实际格式：" + std::to_string(rgb_frame->format) + "）";
        return false;
    }
    if (rgb_frame->width <= 0 || rgb_frame->height <= 0) {
        error_msg_ = "保存JPG失败：帧宽高无效";
        return false;
    }
    cv::Mat rgb_mat(
                    rgb_frame->height,          // 图像高度
                    rgb_frame->width,           // 图像宽度
                    CV_8UC3,                    // 数据类型：8位无符号，3通道
                    rgb_frame->data[0],         // 像素数据起始地址
                    rgb_frame->linesize[0]);    // 每行数据的字节数（对齐用）
    
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

bool FFmpegDecoder::resizeRGBFrameWithBlank(const AVFrame *src_rgb, AVFrame *dst_rgb, int dst_w, int dst_h){
    // 1. 入参检验
    if (!src_rgb || !dst_rgb) {
        error_msg_ = "缩放失败：输入/输出帧为空！！！";
        return false;
    }
    
    if (src_rgb->format != AV_PIX_FMT_RGB24) {
        error_msg_ = "缩放失败：输入帧不是RGB24格式（实际格式：" + std::to_string(src_rgb->format) + "）";
        return false;
    }
    
    cv::Mat src_mat(
                    src_rgb->height,
                    src_rgb->width,
                    CV_8UC3,
                    src_rgb->data[0],
                    src_rgb->linesize[0]
                    );
    
    // OpenCV默认处理BGR，需转换（避免颜色异常）
    cv::Mat bgr_mat;
    cv::cvtColor(src_mat, bgr_mat, cv::COLOR_RGB2BGR);
    
    // 3. 缩放：保持原图比例，边缘填充黑色（避免拉伸）
    cv::Mat resized_bgr;
    double scale = std::min((double)dst_w/bgr_mat.cols,(double)dst_h/bgr_mat.rows);
    int new_w = bgr_mat.cols * scale;
    int new_h = bgr_mat.rows * scale;
    cv::resize(bgr_mat, resized_bgr, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR); // 线性插值，画质更优
    
    // 4. 填充到目标尺寸（居中放置，边缘补黑）
    cv::Mat dst_bgr(dst_h,dst_w,CV_8UC3,cv::Scalar(0,0,0)); //黑色背景
    cv::Rect roi((dst_w - new_w)/2,(dst_h - new_h) / 2, new_w, new_h); //居中 ROI
    resized_bgr.copyTo(dst_bgr(roi));
    
    // 5. cv::Mat→AVFrame（RGB24格式）
    // 确保输出帧缓冲区有效
    if (dst_rgb->width != dst_w || dst_rgb->height != dst_h || dst_rgb->format != AV_PIX_FMT_RGB24) {
        av_frame_unref(dst_rgb);
        dst_rgb->width = dst_w;
        dst_rgb->height = dst_h;
        dst_rgb->format = AV_PIX_FMT_RGB24;
        if (av_frame_get_buffer(dst_rgb, 32) < 0) {
            error_msg_ = "缩放失败：输出帧缓冲区分配失败";
            return false;
        }
    }
    
    // BGR→RGB，复制数据到AVFrame
    cv::Mat dst_rgb_mat;
    cv::cvtColor(dst_bgr, dst_rgb_mat, cv::COLOR_BGR2RGB);
    for (int i = 0; i < dst_h; ++i) {
        memcpy(
               dst_rgb->data[0] + i * dst_rgb->linesize[0], // 目标行地址
               dst_rgb_mat.data + i * dst_rgb_mat.step,      // 源行地址
               dst_w * 3                                     // 每行字节数（RGB24：1像素3字节）
               );
    }
    return true;
}

bool FFmpegDecoder::resizeRGBFrame(const AVFrame *src_rgb, AVFrame *dst_rgb, int dst_w, int dst_h){
    // 1. 入参检验
    if (!src_rgb || !dst_rgb) {
        error_msg_ = "缩放失败：输入/输出帧为空！！！";
        return false;
    }
    
    if (src_rgb->format != AV_PIX_FMT_RGB24) {
        error_msg_ = "缩放失败：输入帧不是RGB24格式（实际格式：" + std::to_string(src_rgb->format) + "）";
        return false;
    }
    
    cv::Mat src_mat(
                    src_rgb->height,
                    src_rgb->width,
                    CV_8UC3,
                    src_rgb->data[0],
                    src_rgb->linesize[0]
                    );
    
    // OpenCV默认处理BGR，需转换（避免颜色异常）
    cv::Mat bgr_mat;
    cv::cvtColor(src_mat, bgr_mat, cv::COLOR_RGB2BGR);
    
    // 先裁剪原始黑边
    cv::Mat cropped_mat = cropBlackEdges(bgr_mat);
    if (cropped_mat.empty()) {
        error_msg_ = "裁剪黑边后图像为空";
        return false;
    }
    
    // 调整比例：裁剪至与目标尺寸相同的宽高比，避免填充黑边
    int src_cropped_w = cropped_mat.cols;
    int src_cropped_h = cropped_mat.rows;
    double target_ratio = (double)dst_w / dst_h; // 目标比例（如224/224=1）
    double src_ratio = (double)src_cropped_w / src_cropped_h;
    
    cv::Mat ratio_adjusted_mat;
    if (src_ratio > target_ratio) {
        // 原图更宽：裁剪左右两侧
        int new_w = src_cropped_h * target_ratio;
        int x = (src_cropped_w - new_w) / 2;
        // 确保裁剪区域有效（避免x为负或new_w超出范围）
        x = std::max(0, x);
        new_w = std::min(new_w, src_cropped_w - x);
        ratio_adjusted_mat = cropped_mat(cv::Rect(x, 0, new_w, src_cropped_h));
    } else {
        // 原图更高：裁剪上下两侧
        int new_h = src_cropped_w / target_ratio;
        int y = (src_cropped_h - new_h) / 2;
        // 确保裁剪区域有效
        y = std::max(0, y);
        new_h = std::min(new_h, src_cropped_h - y);
        ratio_adjusted_mat = cropped_mat(cv::Rect(0, y, src_cropped_w, new_h));
    }
    
    // 直接缩放到目标尺寸（无填充，无黑边）
    cv::Mat resized_bgr;
    cv::resize(ratio_adjusted_mat, resized_bgr, cv::Size(dst_w, dst_h), 0, 0, cv::INTER_LINEAR);
    
    // 确保输出帧缓冲区有效
    if (dst_rgb->width != dst_w || dst_rgb->height != dst_h || dst_rgb->format != AV_PIX_FMT_RGB24) {
        av_frame_unref(dst_rgb);
        dst_rgb->width = dst_w;
        dst_rgb->height = dst_h;
        dst_rgb->format = AV_PIX_FMT_RGB24;
        if (av_frame_get_buffer(dst_rgb, 32) < 0) {
            error_msg_ = "缩放失败：输出帧缓冲区分配失败";
            return false;
        }
    }
    // BGR→RGB，复制数据到AVFrame
    cv::Mat dst_rgb_mat;
    cv::cvtColor(resized_bgr, dst_rgb_mat, cv::COLOR_BGR2RGB);
    for (int i = 0; i < dst_h; ++i) {
        memcpy(
               dst_rgb->data[0] + i * dst_rgb->linesize[0], // 目标行地址
               dst_rgb_mat.data + i * dst_rgb_mat.step,      // 源行地址
               dst_w * 3                                     // 每行字节数（RGB24：1像素3字节）
               );
    }
    
    return true;
}

bool FFmpegDecoder::normalizeRGBFrame(const AVFrame* rgb_frame, float* output_buf,
                                      const std::vector<float>& mean,
                                      const std::vector<float>& std) {
    // 入参校验（保留核心检查）
    if (!rgb_frame || !output_buf || rgb_frame->format != AV_PIX_FMT_RGB24) {
        error_msg_ = "归一化失败：输入帧无效或格式不是RGB24";
        return false;
    }
    if (std[0] == 0 || std[1] == 0 || std[2] == 0) {
        error_msg_ = "归一化失败：标准差不能为0";
        return false;
    }
    
    const int frame_w = rgb_frame->width;
    const int frame_h = rgb_frame->height;
    const int channel_size = frame_w * frame_h; // 单通道像素数
    
    // 1. 预计算常数倒数（除法转乘法，提升速度）
    const float inv_255 = 1.0f / 255.0f;
    const float inv_std[3] = {1.0f / std[0], 1.0f / std[1], 1.0f / std[2]};
    const float mean_vals[3] = {mean[0], mean[1], mean[2]};
    
    // 2. 按行遍历（缓存友好：连续内存访问 NHWC格式：RRRGGGBBB....）
    for (int h = 0; h < frame_h; ++h) {
        // 行数据地址
        const uint8_t* row_data = rgb_frame->data[0] + h * rgb_frame->linesize[0];
        // 当前行在单通道中的起始索引
        const int row_base = h * frame_w;
        
        // 3. 循环展开（处理4个像素/次，减少循环次数）
        int w = 0;
        for (; w < frame_w - 3; w += 4) {
            // 一次性读取4个像素的RGB数据（共12字节）
            const uint8_t* pixel = row_data + w * 3;
            
            // 计算4个像素的R通道（并行处理）
            output_buf[row_base + w]     = (pixel[0] * inv_255 - mean_vals[0]) * inv_std[0];
            output_buf[row_base + w + 1] = (pixel[3] * inv_255 - mean_vals[0]) * inv_std[0];
            output_buf[row_base + w + 2] = (pixel[6] * inv_255 - mean_vals[0]) * inv_std[0];
            output_buf[row_base + w + 3] = (pixel[9] * inv_255 - mean_vals[0]) * inv_std[0];
            
            // 计算4个像素的G通道
            output_buf[channel_size + row_base + w]     = (pixel[1] * inv_255 - mean_vals[1]) * inv_std[1];
            output_buf[channel_size + row_base + w + 1] = (pixel[4] * inv_255 - mean_vals[1]) * inv_std[1];
            output_buf[channel_size + row_base + w + 2] = (pixel[7] * inv_255 - mean_vals[1]) * inv_std[1];
            output_buf[channel_size + row_base + w + 3] = (pixel[10] * inv_255 - mean_vals[1]) * inv_std[1];
            
            // 计算4个像素的B通道
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

