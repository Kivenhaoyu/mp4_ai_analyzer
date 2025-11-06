//
//  media_frame.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 28/10/2025.
//

#ifndef MEDIA_FRAME_H
#define MEDIA_FRAME_H

#include <cstdint>
#include <vector>
#include <memory>
#include <string>

struct AVFrame;

enum class MediaType {
    VIDEO,
    AUDIO,
    UNKNOWN,
};


// 视频格式（视频）
enum class PixelFormat {
    UNKNOWN,   // 未知格式
    YUV420P,   // 最常用的解码输出格式（YUV420P）
    BGR24,     // 适合渲染/UI的BGR格式（24位）
    RGB24,     // 适合AI模型的RGB格式（24位）
    UYVY422    // 可能用于摄像头输入的YUV422格式
};

// 采样格式（音频）
enum class SampleFormat {
    UNKNOWN,
    S16,        // 16位有符号整数
    FLT,        // 32位浮点数
    S32,        // 32位有符号整数
    U8          // 8位无符号整数
};

/**
 * 像素格式转字符串（调试/日志用）
 * @param fmt 自定义像素格式
 * @return 格式名称字符串
 */
std::string pixelFormatToString(PixelFormat fmt);

/**
 * 音频采样格式转字符串（调试/日志用）
 * @param fmt 自定义采样格式
 * @return 格式名称字符串
 */
std::string SampleFormatToString(SampleFormat fmt);

class MediaFrame {
public:
    MediaFrame() = delete;
    
    /**
     * 从FFmpeg的AVFrame创建MediaFrame（核心工厂方法）
     * @param av_frame FFmpeg解码得到的AVFrame（所有权将被接管）
     * @return 共享指针包装的MediaFrame实例
     * @note 调用者无需手动释放av_frame，由MediaFrame内部管理
     */
    static std::shared_ptr<MediaFrame> createFromAVFrame(AVFrame* av_frame);
    
    /**
     * 创建空帧（用于格式转换、处理的输出帧）
     * @param width 帧宽度
     * @param height 帧高度
     * @param fmt 像素格式
     * @return 共享指针包装的空MediaFrame实例（已分配内存）
     */
    static std::shared_ptr<MediaFrame> createEmpty(int width, int height, PixelFormat fmt);
    
    // 析构函数（自动释放内部AVFrame资源）
    ~MediaFrame();
    
    // 禁止拷贝（共享通过shared_ptr实现，避免数据冗余）
    MediaFrame(const MediaFrame&) = delete;
    MediaFrame& operator=(const MediaFrame&) = delete;
    
    // 允许移动（高效传递所有权，如跨线程传递帧）
    MediaFrame(MediaFrame&&) noexcept;
    MediaFrame& operator=(MediaFrame&&) noexcept;
    
    int width() const {return width_;}
    int height() const {return height_;}
    PixelFormat format() const {return format_;}
    int64_t pts() const {return pts_;}
    int64_t pts_ms()const {return pts_ms_;}
    void setPts(int64_t pts);                       // 设置原始PTS并自动更新毫秒值
    
    /**
     * 设置时间基准（来自AVStream->time_base）
     * @param num 时间基准分子（如1）
     * @param den 时间基准分母（如90000）
     * @note 必须在设置PTS前调用，否则pts_ms_计算错误
     */
    void setTimeBase(int num, int den);
    /**
     * 获取像素数据指针（按平面存储）
     * @return 数据指针数组（如YUV420P包含Y/U/V三个平面）
     */
    const std::vector<uint8_t*>& data() const { return data_; }
    
    /**
     * 获取每行字节数（对齐用）
     * @return 每行字节数数组（与data()对应）
     */
    const std::vector<int>& linesize() const { return linesize_; }
    
    /**
     * 获取内部封装的AVFrame（仅用于与FFmpeg工具交互）
     * @note 外部模块禁止调用，避免依赖FFmpeg细节
     */
    AVFrame* getAVFrame() const { return av_frame_; }
    
private:
    
    explicit MediaFrame(AVFrame* av_frame);
    MediaFrame(int width, int height, PixelFormat fmt);
    
    // 初始化数据指针
    void initDataPointers();
    
    // 释放内部资源
    void releaseResources();
    
    // FFmpeg与自定义像素格式互转（内部工具）
    static PixelFormat avPixelFormatToCustom(int av_fmt);
    static int customPixelFormatToAV(PixelFormat fmt);
    
    int width_ = 0;
    int height_ = 0;
    PixelFormat format_ = PixelFormat::UNKNOWN;
    
    //时间戳
    int64_t pts_ = -1;       //原始PTS（FFmpeg的time_base单位）
    int64_t pts_ms_ = -1;    // 转换后的毫秒级PTS
    int time_base_num_ = 1;  // 时间基准分子
    int time_base_den_ = 1;  // 时间基准分母
    
    //像素数据 （引用AVFrame的数据，无拷贝）
    std::vector<uint8_t*> data_;    // 数据指针数组（按平面存储）
    std::vector<int> linesize_;     // 每行字节数数组（与data对应）
    
    AVFrame* av_frame_ = nullptr;   // 内部封装的FFmpeg AVFrame（对外隐藏）
};

using MediaFramePtr = std::shared_ptr<MediaFrame>;

#endif /* MEDIA_FRAME_H */
