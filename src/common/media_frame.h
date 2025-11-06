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
    NV12,
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
std::string PixelFormatToString(PixelFormat fmt);

/**
 * 音频采样格式转字符串（调试/日志用）
 * @param fmt 自定义采样格式
 * @return 格式名称字符串
 */
std::string SampleFormatToString(SampleFormat fmt);

class MediaFrame {
public:
    using Ptr = std::shared_ptr<MediaFrame>;
    using WeakPtr = std::weak_ptr<MediaFrame>;
    
    virtual ~MediaFrame() = default;
    
    //禁止拷贝
    MediaFrame(const MediaFrame&) = delete;
    MediaFrame& operator = (const MediaFrame&) = delete;
    
    //允许移动
    MediaFrame(MediaFrame&&) noexcept = default;
    MediaFrame& operator = (MediaFrame &&) noexcept = default;
    
    //通用属性访问
    MediaType type() const { return type_;}
    int64_t pts() const { return pts_;}
    int64_t dts() const { return dts_;}
    int duration() const { return duration_;}
    int streamIndex() const { return stream_idx_;}
    bool isShallowCopy() const { return shallow_copy_;}
    
    //通用属性设置
    void setPts(int64_t pts){ pts_ = pts;}
    void setDts(int64_t dts){ dts_ = dts;}
    void setDuration(int duration) { duration_ = duration;}
    void setStreamIndex(int index) { stream_idx_ = index;}
    void setShallowCopy(int flag) { shallow_copy_ = flag;}
    
    virtual std::string debugInfo() const = 0;
    
protected:
    
    explicit MediaFrame(MediaType type):type_(type) {}
    
private:
    //媒体类型
    MediaType type_ = MediaType::UNKNOWN;
    int64_t pts_ = -1;          // 显示时间戳
    int64_t dts_ = -1;          // 解码时间戳
    int duration_ = 0;          // 持续时长
    int stream_idx_ = -1;       // 所属流索引
    bool shallow_copy_ = false; //标记是否是浅拷贝 AVFrame
};

class VideoFrame : public MediaFrame {
public:
    using Ptr = std::shared_ptr<VideoFrame>;
    /**
     * 从FFmpeg的AVFrame创建MediaFrame（核心工厂方法）
     * @param av_frame FFmpeg解码得到的AVFrame（所有权将被接管）
     * @return 共享指针包装的MediaFrame实例
     * @note 调用者无需手动释放av_frame，由MediaFrame内部管理
     */
    static Ptr create(int width, int height, PixelFormat fmt) {
        return std::make_shared<VideoFrame>(width,height,fmt);
    }
    
    int width() const { return  width_;}
    int height() const { return height_;}
    PixelFormat pixelFormat() const {return pix_fmt_;}
    const std::vector<uint8_t*>& data() const {return data_;}
    const std::vector<int>& linesize() const {return linesize_;}
    
    // 属性设置
    void setWidth(int width) { width_ = width;}
    void setHeight(int height) { height_ = height;}
    void setPixelFormat(PixelFormat fmt) {pix_fmt_ = fmt;}
    void setData(const std::vector<uint8_t*>& data, const std::vecotr<int>& linesize) {
        data_ = data;
        linesize_ = linesize;
    }
    
    // 分配视频缓冲区（深拷贝时用）
    bool allocateBuffers();
    
    // 释放缓冲区（深拷贝自有数据）
    void freeBuffer();
    
    std::string debugInfo() const override {
        std::stringstream ss;
        ss << "VideoFrame: "
        << "stream=" << stream_index() << ", "
        << "w=" << width_ << ", h=" << height_ << ", "
        << "fmt=" << PixelFormatToString(pix_fmt_) << ", "
        << "pts=" << pts() << ", dts=" << dts() << ", "
        << "shallow=" << (isShallowCopy() ? "yes" : "no");
        return ss.str();
    }
    
private:
    VideoFrame(int width, int height, PixelFormat fmt) : MediaFrame(MediaType::VIDEO), width_(width), height_(height), pix_fmt_(fmt) {}
    int width_ = 0;
    int height_ = 0;
    PixelFormat pix_fmt_ = PixelFormat::UNKNOWN;
    std::vector<uint8_t*>data_;  //数据平面指针
    std::vector<int>linesize_;  //每行字节数
};


class AudioFrame : public MediaFrame {
public:
    using Ptr = std::shared_ptr<AudioFrame>;
    
    static Ptr create(int sample_rate, int channels, SampleFormat fmt, int nb_samples) {
        return std::make_shared<AudioFrame>(sample_rate,channels,fmt,nb_samples);
    };
    
    //属性访问
    int sampleRate() const { return sample_rate_;}
    int channels() const { return channels_;}
    SampleFormat sampleFormat() const { return sample_format_;}
    int nbSamples() const { return nb_samples_;}
    const uint8_t* data() const { return data_;}
    const int dataSize() const { return data_size_;}
    
    // 设置对应值
    void setSampleRate(int rate) { sample_rate_ = rate;}
    void setChannels(int channels) { channels_ = channels;}
    void setSampleFromat(SampleFormat fmt) {sample_format_ = fmt;}
    void setNbSamples(int samples) { nb_samples_ = samples;}
    void setData(uint8_t* data, const int size) {
        data_ = data;
        data_size_ = size;
    }
    
    
    // 分配缓冲区（深拷贝时用）
    bool allocateBuffers();
    
    // 释放缓冲区（深拷贝自有数据）
    void freeBuffer();
    
    // 调试信息
    std::string debugInfo() const override {
        std::stringstream ss;
        ss << "AudioFrame: "
        << "stream=" << streamIndex() << ", "
        << "rate=" << sample_rate_ << ", ch=" << channels_ << ", "
        << "fmt=" << SampleFormatToString(sample_format_) << ", "
        << "samples=" << nb_samples_ << ", "
        << "pts=" << pts() << ", dts=" << dts() << ", "
        << "shallow=" << (isShallowCopy() ? "yes" : "no");
        return ss.str();
    }
    
private:
    AudioFrame(int sample_rate, int channels, SampleFormat fmt,int nb_samples):MediaFrame(MediaType::AUDIO),sample_rate_(sample_rate),channels_(channels),sample_format_(fmt),nb_samples_(nb_samples) {}
    
    int sample_rate_ = 0;       // 采样率
    int channels_ = 0;          // 声道数
    SampleFormat sample_format_ = SampleFormat::UNKNOWN;  // 采样格式
    int nb_samples_ = 0;        // 每帧采样数
    uint8_t* data_ = nullptr;    // 音频数据大小（线性缓冲）
    int data_size_ = 0;          // 数据大小（字节）
    
};
#endif /* MEDIA_FRAME_H */
