//
//  main.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 4/10/2025.
//

#include <stdio.h>
#include <memory>
// 时间统计必备（C++11及以上支持，几乎所有编译器都兼容）
#include <chrono>
// 用于控制台输出格式（可选，让结果更整齐）
#include <iomanip>
#include <thread>

#include "decoder.h"
#include "frame_pool.h"
#include "frame_guard.h"
#include "ai_infer.h"
#include "cv_renderer.h"
#include "safe_queue.h"
#include "data_struct.h"
using namespace std;

void testCamera() {
    FFmpegDecoder decoder;
    std::string camera_path = "0";
    
    if (!decoder.openWithDevice(camera_path, true)) {
        std::cerr << "摄像头打开失败：" << decoder.getErrorMsg() << std::endl;
        return;
    }
    int width = decoder.getVideoWidth();
    int height = decoder.getVideoHeight();
    std::cout << "摄像头打开成功！宽=" << width
    << "，高=" << height
    << "，目标帧率=" << decoder.getVideoCodecName() << std::endl;
    
    CVFrameRenderer renderer("AI Camera Window", width / 2, height / 2); // 窗口名+初始尺寸
    
    AVFramePool yuvpool(3,width,height,AV_PIX_FMT_UYVY422);
    AVFramePool rgbpool(3,width,height,AV_PIX_FMT_RGB24);
    AVFramePool resizedpool(3,224,224,AV_PIX_FMT_RGB24);
    // 分配输出缓冲区（224×224×3=150528个float）
    const int model_input_size = 224 * 224 * 3;
    float* model_input = new float[model_input_size];
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 初始化 AI 推理器
    AIInfer ai_infer;
    std::string model_path = "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/models/mobilenetv2-12.onnx";
    //    std::string model_path = "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/models/resnet50-v1-7.onnx";
    if (!ai_infer.init(model_path)) {
        std::cerr << "AI模型初始化失败，退出测试" << std::endl;
        delete[] model_input;  // 提前释放缓冲区
        decoder.close();
        return;
    }
    
    //AI模型归一化参数
    const std::vector<float> mean = {0.485f, 0.456f, 0.406f};  // R, G, B 三个通道的均值
    const std::vector<float> std = {0.229f, 0.224f, 0.225f};   // 三个通道的标准差
    long long i = 0;
    while(!renderer.shouldQuit()) {
        i++;
        //        auto yuv_frame = yuvpool.getFrame();
        //        auto rgb_frame = rgbpool.getFrame(); // 用于接收RGB数据
        //        auto resize_frame = resizedpool.getFrame();
        FrameGuard<AVFramePool> yuv_guard(yuvpool);
        FrameGuard<AVFramePool> rgb_guard(rgbpool);
        FrameGuard<AVFramePool> resize_guard(resizedpool);
        
        // 2. 通过 guard 获取帧指针（无需手动调用 getFrame()）
        auto* yuv_frame = yuv_guard.get().get();  // 注意：双层 get()
        auto* rgb_frame = rgb_guard.get().get();
        auto* resize_frame = resize_guard.get().get();
        
        // 打印当前耗时
        auto start = std::chrono::high_resolution_clock::now();
        bool step_success = true;
        
        // 1. 解码耗时
        auto decode_start = std::chrono::high_resolution_clock::now();
        if (!decoder.getFrame(yuv_frame)) {
            std::cerr << "解码第" << i << "帧失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        auto decode_end = std::chrono::high_resolution_clock::now();
        double decode_ms = std::chrono::duration<double,milli>(decode_end - decode_start).count();
        
        std::cout << "解码摄像头第" << i << "帧成功" << std::endl;
        auto conver_start = std::chrono::high_resolution_clock::now();
        if (!decoder.converUYUV422ToRgb(yuv_frame, rgb_frame)) {
            std::cerr << "第" << i << "帧转换失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        auto conver_end = std::chrono::high_resolution_clock::now();
        double conver_ms = std::chrono::duration<double,milli>(conver_end - conver_start).count();
        
        std::cout << "第" << i << "帧 UYVY422→RGB 转换成功" << std::endl;
        //                // （可选）保存RGB帧为图片，验证效果
        //                std::string save_path = "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/data/camera_frame_" + std::to_string(i) + ".jpg";
        //                if (decoder.saveRGBFrameToJPG(rgb_frame.get(), save_path)) {
        //                    std::cout << "第" << i << "帧已保存至：" << save_path << std::endl;
        //                } else {
        //                    std::cerr << "保存失败：" << decoder.getErrorMsg() << std::endl;
        //                }
        
        auto resize_start = std::chrono::high_resolution_clock::now();
        if (!decoder.resizeRGBFrame(rgb_frame, resize_frame)) {
            std::cerr << "第" << i << "帧缩放失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        auto resize_end = std::chrono::high_resolution_clock::now();
        double resize_ms = std::chrono::duration<double,milli>(resize_end - resize_start).count();
        
        //        // 保存缩放后的JPG，检查尺寸和画质
        //        decoder.saveRGBFrameToJPG(resize_frame, "resized_frame_" + std::to_string(i) + ".jpg");
        //        std::cout << "缩放后帧尺寸：" << resize_frame->width << "×" << resize_frame->height << std::endl;
        //
        
        auto normalize_start = std::chrono::high_resolution_clock::now();
        if (!decoder.normalizeRGBFrame(resize_frame, model_input, mean, std)) {
            std::cerr << "第" << i << "帧归一化失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        auto normalize_end = std::chrono::high_resolution_clock::now();
        double normalize_ms = std::chrono::duration<double,milli>(normalize_end - normalize_start).count();
        
        //开始推理
        auto ai_start = std::chrono::high_resolution_clock::now();
        AIResult ai_result = ai_infer.infer(model_input, model_input_size);
        auto ai_end = std::chrono::high_resolution_clock::now();
        double ai_ms = std::chrono::duration<double,milli>(ai_end - ai_start).count();
        
        //渲染画面显示结果
        
        string render_text = ai_result.is_valid ?
                    (ai_result.class_name + " | confidence:" + std::to_string(ai_result.confidence).substr(0, 4)) :
                    "Unrecognized";
        renderer.render(
                        rgb_frame->data[0],  // RGB数据
                        rgb_frame->width,    // 宽度
                        rgb_frame->height,   // 高度
                        render_text          // 叠加文字
                        );
        
        //打印AI推理结果
        std::cout << "【第" << i << "帧 AI结果】";
        if (ai_result.is_valid) {
            std::cout << "类别：" << ai_result.class_name
            << " | 置信度：" << fixed << setprecision(2) << ai_result.confidence;
        } else {
            std::cout << "未识别到有效物体（置信度：" << fixed << setprecision(2) << ai_result.confidence << "）";
        }
        std::cout << " | 推理耗时：" << ai_ms << "ms" << std::endl;
        
        // 强制延迟：保证每帧间隔≥33ms（匹配30FPS）
        auto end = std::chrono::high_resolution_clock::now();
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (cost < 33) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33 - cost));
        }
        std::cout << "第" << i << "帧：解码=" << decode_ms << "ms，转换=" << conver_ms << "ms，缩放=" << resize_ms << "ms，归一=" << normalize_ms << "ms，AI=" << ai_ms << "ms，总=" << cost << "ms" << std::endl;
    }
    ai_infer.destroy();
    delete[] model_input; // 释放缓冲区
    decoder.close();
}

void testLocalFile(string& file_path) {
    FFmpegDecoder decoder;
    bool ret = decoder.openWithLocalFile(file_path);
    if (!ret) {
        cout << decoder.getErrorMsg() << endl;
    }else {
        cout << "文件打开成功:" << decoder.getVideoWidth() << ":" << decoder.getVideoHeight() << "编码格式：" << decoder.getVideoCodecName() << endl;
    }
    
    //    auto frame_deleter = [](AVFrame *f) {
    //        if (f != nullptr) {
    //            av_frame_free(&f);
    //        }
    //    };
    //
    //    unique_ptr<AVFrame, decltype(frame_deleter) > frame(av_frame_alloc(),frame_deleter);
    //    ret = decoder->getFrame(frame.get());
    //    if (!ret) {
    //        cout << "获取首帧失败" << endl;
    //        cout << decoder->getErrorMsg() << endl;
    //    }else {
    //        cout << "获取首帧成功" << endl;
    //    }
    int width = decoder.getVideoWidth();
    int height = decoder.getVideoHeight();
    std::cout << "文件打开成功！宽=" << width
    << "，高=" << height
    << "，目标帧率=" << decoder.getVideoCodecName() << std::endl;
    
    CVFrameRenderer renderer("AI Local File Window", width / 2, height / 2); // 窗口名+初始尺寸
    
    AVFramePool yuvpool(3,width,height,AV_PIX_FMT_YUV420P);
    AVFramePool rgbpool(3,width,height,AV_PIX_FMT_RGB24);
    AVFramePool resizedpool(3,224,224,AV_PIX_FMT_RGB24);
    // 分配输出缓冲区（224×224×3=150528个float）
    const int model_input_size = 224 * 224 * 3;
    float* model_input = new float[model_input_size];
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 初始化 AI 推理器
    AIInfer ai_infer;
    std::string model_path = "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/models/mobilenetv2-12.onnx";
    if (!ai_infer.init(model_path)) {
        std::cerr << "AI模型初始化失败，退出测试" << std::endl;
        delete[] model_input;  // 提前释放缓冲区
        decoder.close();
        return;
    }
    
    //AI模型归一化参数
    const std::vector<float> mean = {0.485f, 0.456f, 0.406f};  // R, G, B 三个通道的均值
    const std::vector<float> std = {0.229f, 0.224f, 0.225f};   // 三个通道的标准差
    long long i = 0;
    while (!renderer.shouldQuit()) {
        i++;
        //        auto yuv_frame = yuvpool.getFrame();
        //        auto rgb_frame = rgbpool.getFrame(); // 用于接收RGB数据
        //        auto resize_frame = resizedpool.getFrame();
        FrameGuard<AVFramePool> yuv_guard(yuvpool);
        FrameGuard<AVFramePool> rgb_guard(rgbpool);
        FrameGuard<AVFramePool> resize_guard(resizedpool);
        
        // 2. 通过 guard 获取帧指针（无需手动调用 getFrame()）
        auto* yuv_frame = yuv_guard.get().get();  // 注意：双层 get()
        auto* rgb_frame = rgb_guard.get().get();
        auto* resize_frame = resize_guard.get().get();
        
        // 打印当前耗时
        auto start = std::chrono::high_resolution_clock::now();
        bool step_success = true;
        
        // 1. 解码耗时
        auto decode_start = std::chrono::high_resolution_clock::now();
        if (!decoder.getFrame(yuv_frame)) {
            std::cerr << "解码结束！！！" << decoder.getErrorMsg() << std::endl;
            break;
        }
        auto decode_end = std::chrono::high_resolution_clock::now();
        double decode_ms = std::chrono::duration<double,milli>(decode_end - decode_start).count();
        
        std::cout << "解码本地文件第" << i << "帧成功" << std::endl;
        auto conver_start = std::chrono::high_resolution_clock::now();
        if (!decoder.convertYuvToRgb(yuv_frame, rgb_frame)) {
            std::cerr << "第" << i << "帧转换失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        auto conver_end = std::chrono::high_resolution_clock::now();
        double conver_ms = std::chrono::duration<double,milli>(conver_end - conver_start).count();
        
        std::cout << "第" << i << "帧 YUV420→RGB 转换成功" << std::endl;
        //        // （可选）保存RGB帧为图片，验证效果
        //        std::string save_path = "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/data/camera_frame_" + std::to_string(i) + ".jpg";
        //        if (decoder.saveRGBFrameToJPG(rgb_frame, save_path)) {
        //            std::cout << "第" << i << "帧已保存至：" << save_path << std::endl;
        //        } else {
        //            std::cerr << "保存失败：" << decoder.getErrorMsg() << std::endl;
        //        }
        
        auto resize_start = std::chrono::high_resolution_clock::now();
        if (!decoder.resizeRGBFrameWithBlank(rgb_frame, resize_frame)) {
            std::cerr << "第" << i << "帧缩放失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        auto resize_end = std::chrono::high_resolution_clock::now();
        double resize_ms = std::chrono::duration<double,milli>(resize_end - resize_start).count();
        
        //        // 保存缩放后的JPG，检查尺寸和画质
        //        decoder.saveRGBFrameToJPG(resize_frame, "resized_frame_" + std::to_string(i) + ".jpg");
        //        std::cout << "缩放后帧尺寸：" << resize_frame->width << "×" << resize_frame->height << std::endl;
        
        
        auto normalize_start = std::chrono::high_resolution_clock::now();
        if (!decoder.normalizeRGBFrame(resize_frame, model_input, mean, std)) {
            std::cerr << "第" << i << "帧归一化失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        auto normalize_end = std::chrono::high_resolution_clock::now();
        double normalize_ms = std::chrono::duration<double,milli>(normalize_end - normalize_start).count();
        
        //开始推理
        auto ai_start = std::chrono::high_resolution_clock::now();
        AIResult ai_result = ai_infer.infer(model_input, model_input_size);
        auto ai_end = std::chrono::high_resolution_clock::now();
        double ai_ms = std::chrono::duration<double,milli>(ai_end - ai_start).count();
        
        //渲染画面显示结果
        
        string render_text = ai_result.is_valid ?
                    (ai_result.class_name + " | confidence:" + std::to_string(ai_result.confidence).substr(0, 4)) :
                    "Unrecognized";
        renderer.render(
                        rgb_frame->data[0],  // RGB数据
                        rgb_frame->width,    // 宽度
                        rgb_frame->height,   // 高度
                        render_text          // 叠加文字
                        );
        
        //打印AI推理结果
        std::cout << "【第" << i << "帧 AI结果】";
        if (ai_result.is_valid) {
            std::cout << "类别：" << ai_result.class_name
            << " | 置信度：" << fixed << setprecision(2) << ai_result.confidence;
        } else {
            std::cout << "未识别到有效物体（置信度：" << fixed << setprecision(2) << ai_result.confidence << "）";
        }
        std::cout << " | 推理耗时：" << ai_ms << "ms" << std::endl;
        
        // 强制延迟：保证每帧间隔≥33ms（匹配30FPS）
        auto end = std::chrono::high_resolution_clock::now();
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "第" << i << "帧：解码=" << decode_ms << "ms，转换=" << conver_ms << "ms，缩放=" << resize_ms << "ms，归一=" << normalize_ms << "ms，AI=" << ai_ms << "ms，总=" << cost << "ms" << std::endl;
    }
    ai_infer.destroy();
    delete[] model_input; // 释放缓冲区
    decoder.close();
    std::cout << "本地视频测试结束" << std::endl;
}

void testCameraWith25FPS() {
    FFmpegDecoder decoder;
    std::string camera_path = "0";
    if (!decoder.openWithDevice(camera_path, true)) {
        std::cerr << "摄像头打开失败：" << decoder.getErrorMsg() << std::endl;
        return;
    }
    
    AVFrame* yuv_frame = av_frame_alloc();
    int frame_count = 0;       // 总解码帧数
    int output_count = 0;      // 输出帧数（目标25FPS）
    
    while (output_count < 100) {  // 输出100帧25FPS数据
        if (decoder.getFrame(yuv_frame)) {
            frame_count++;
            // 核心：每6帧丢弃1帧（30FPS → 25FPS：30/6=5，丢弃1帧后剩5，5*5=25）
            if (frame_count % 6 != 0) {  // 保留第1-5帧，丢弃第6帧
                output_count++;
                std::cout << "输出第" << output_count << "帧（原始第" << frame_count << "帧）" << std::endl;
                // 这里添加你的处理逻辑（如格式转换、输入AI模型）
            }
        }
    }
    
    av_frame_free(&yuv_frame);
    decoder.close();
}

// 测试阻塞模式（确保所有数据不丢失）
void testBlockPolicy() {
    std::cout << "\n===== 测试阻塞模式（不丢数据） =====" << std::endl;
    SafeQueue<int> q(QueuePolicy::BLOCK_WHEN_FULL, 50); // 最大容量50，满时阻塞
    std::atomic<bool> running(true);
    const int total = 1000; // 总数据量

    // 生产者：推送0-999（速度较快）
    std::thread producer([&]() {
        for (int i = 0; i < total && running; ++i) {
            q.push(i);
            // 生产者每100微秒推1个（速度快于消费者）
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        running = false;
        std::cout << "阻塞模式-生产者结束" << std::endl;
    });

    // 消费者：处理数据（速度较慢）
    std::thread consumer([&]() {
        int val;
        int expected = 0;
        while (true) {
            bool has_data = q.pop(val, 200); // 超时200ms
            if (has_data) {
                if (val != expected) {
                    std::cerr << "阻塞模式-数据错误：预期" << expected << "，实际" << val << std::endl;
                } else {
                    // 每100个打印一次（减少输出量）
                    if (expected % 100 == 0) {
                        std::cout << "阻塞模式-处理到：" << expected << std::endl;
                    }
                }
                expected++;
            } else {
                // 退出条件：生产者停止且队列空
                if (!running && q.size() == 0) {
                    break;
                }
            }
        }
        std::cout << "阻塞模式-消费者结束，共处理" << expected << "个数据（应等于1000）" << std::endl;
    });

    producer.join();
    consumer.join();
}

// 测试丢弃模式（允许丢旧数据，只保留最新的）
void testDropPolicy() {
    std::cout << "\n===== 测试丢弃模式（丢旧数据） =====" << std::endl;
    SafeQueue<int> q(QueuePolicy::DROP_OLD_WHEN_FULL, 5); // 最大容量50，满时丢旧数据
    std::atomic<bool> running(true);
    const int total = 1000; // 总数据量

    // 生产者：推送0-999（速度远快于消费者）
    std::thread producer([&]() {
        for (int i = 0; i < total && running; ++i) {
            q.push(i);
            // 生产者每10微秒推1个（极快，确保队列满）
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        running = false;
        std::cout << "丢弃模式-生产者结束" << std::endl;
    });

    // 消费者：处理数据（速度较慢）
    std::thread consumer([&]() {
        int val;
        int last_val = -1; // 记录最后处理的值（应逐渐增大）
        int count = 0;     // 处理总数

        while (true) {
            bool has_data = q.pop(val, 200);
            if (has_data) {
                count++;
                // 验证数据是递增的（丢弃旧数据后，剩余数据应连续）
                if (val <= last_val) {
                    std::cerr << "丢弃模式-数据错误：当前" << val << "，上一个" << last_val << std::endl;
                }
                last_val = val;
                // 每10个打印一次
                if (count % 10 == 0) {
                    std::cout << "丢弃模式-处理到第：" << val << "（累计" << count << "个）" << std::endl;
                }
            } else {
                if (!running && q.size() == 0) {
                    break;
                }
            }
        }

        // 最终处理的应是最后50个数据（950-999）
        std::cout << "丢弃模式-消费者结束，共处理" << count << "个数据（缓存约50个），最后值：" << last_val << std::endl;
    });

    producer.join();
    consumer.join();
}

int main() {
    
//    // 先测试阻塞模式（确保完整接收）
//    testBlockPolicy();
//    // 再测试丢弃模式（允许丢旧数据）
//    testDropPolicy();

    //    avformat_network_init();
    string file_path = "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/data/天鹅.mp4";
        testLocalFile(file_path);
//        testCamera();
    
    return 0;
}


