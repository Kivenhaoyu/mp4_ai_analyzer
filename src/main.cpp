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

#include "common/decoder/video_decoder.h"
#include "common/util/frame_pool.h"
#include "common/util/frame_guard.h"
#include "common/ai_infer/ai_infer.h"
#include "common/data_structs.h"
#include "common/render/render_factory.h"

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
    // std::cout << "摄像头打开成功！宽=" << width
//    << "，高=" << height
//    << "，目标帧率=" << decoder.getVideoCodecName() << std::endl;
    
    auto renderer = RendererFactory::createRenderer();
    renderer->init("AI Camera Window", width / 2, height / 2);
    
    AVFramePool yuvpool(3,width,height,AV_PIX_FMT_UYVY422);
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
    while(!renderer->should_quit()) {
        i++;
        //        auto yuv_frame = yuvpool.getFrame();
        //        auto rgb_frame = rgbpool.getFrame(); // 用于接收RGB数据
        //        auto resize_frame = resizedpool.getFrame();
        FrameGuard<AVFramePool> yuv_guard(yuvpool);
        FrameGuard<AVFramePool> resize_guard(resizedpool);
        
        // 2. 通过 guard 获取帧指针（无需手动调用 getFrame()）
        auto* yuv_frame = yuv_guard.get().get();  // 注意：双层 get()
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
        
        // std::cout << "解码摄像头第" << i << "帧成功" << std::endl;
        auto conver_start = std::chrono::high_resolution_clock::now();
        if (!decoder.convertCropResizeYuvToBgr(yuv_frame, resize_frame)) {
            std::cerr << "第" << i << "帧转换失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        auto conver_end = std::chrono::high_resolution_clock::now();
        double conver_ms = std::chrono::duration<double,milli>(conver_end - conver_start).count();
        
        //        // 保存缩放后的JPG，检查尺寸和画质
        //        decoder.saveRGBFrameToJPG(resize_frame, "resized_frame_" + std::to_string(i) + ".jpg");
        //        // std::cout << "缩放后帧尺寸：" << resize_frame->width << "×" << resize_frame->height << std::endl;
        //
        
        auto normalize_start = std::chrono::high_resolution_clock::now();
        if (!decoder.normalizeBGRFrame(resize_frame, model_input, mean, std)) {
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
        FrameData frame_data = {resize_frame->data[0],  // RGB数据
            resize_frame->width,    // 宽度
            resize_frame->height,static_cast<double>(resize_frame->pts)};
        renderer->render(frame_data,
                        render_text          // 叠加文字
                        );
        
        //打印AI推理结果
        // std::cout << "【第" << i << "帧 AI结果】";
        if (ai_result.is_valid) {
            // std::cout << "类别：" << ai_result.class_name
//            << " | 置信度：" << fixed << setprecision(2) << ai_result.confidence;
        } else {
            // std::cout << "未识别到有效物体（置信度：" << fixed << setprecision(2) << ai_result.confidence << "）";
        }
        // std::cout << " | 推理耗时：" << ai_ms << "ms" << std::endl;
        
        // 强制延迟：保证每帧间隔≥33ms（匹配30FPS）
        auto end = std::chrono::high_resolution_clock::now();
        auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (cost < 33) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33 - cost));
        }
         std::cout << "第" << i << "帧：解码=" << decode_ms << "ms，归一=" << normalize_ms << "ms，AI=" << ai_ms << "ms，总=" << cost << "ms" << std::endl;
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
    // std::cout << "文件打开成功！宽=" << width
//    << "，高=" << height
//    << "，目标帧率=" << decoder.getVideoCodecName() << std::endl;
    
    auto renderer = RendererFactory::createRenderer();
    renderer->init("AI Camera Window", width / 2, height / 2);
    
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
    while (!renderer->should_quit()) {
        i++;
        FrameGuard<AVFramePool> yuv_guard(yuvpool);
        FrameGuard<AVFramePool> resize_guard(resizedpool);
        
        // 2. 通过 guard 获取帧指针（无需手动调用 getFrame()）
        auto* yuv_frame = yuv_guard.get().get();  // 注意：双层 get()
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
        
        // std::cout << "解码本地文件第" << i << "帧成功" << std::endl;
        auto resize_start = std::chrono::high_resolution_clock::now();
        if (!decoder.convertCropResizeYuvToBgr(yuv_frame, resize_frame)){
            std::cerr << "第" << i << "帧转换失败：" << decoder.getErrorMsg() << std::endl;
            continue;
        }
        
        // 保存缩放后的JPG，检查尺寸和画质
//        decoder.saveBGRFrameToJPG(resize_frame, "resized_frame_" + std::to_string(i) + ".jpg");
        
        auto normalize_start = std::chrono::high_resolution_clock::now();
        if (!decoder.normalizeBGRFrame(resize_frame, model_input, mean, std)) {
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
                    ai_result.class_name :
                    "Unrecognized";
        auto render_start = std::chrono::high_resolution_clock::now();
        FrameData frame_data = {resize_frame->data[0],  // RGB数据
            resize_frame->width,    // 宽度
            resize_frame->height,static_cast<double>(resize_frame->pts)};
        renderer->render(frame_data,
                        render_text          // 叠加文字
                        );
        auto render_end = std::chrono::high_resolution_clock::now();
        double render_ms = std::chrono::duration<double,milli>(render_end - render_start).count();

//        打印AI推理结果
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
         std::cout << "第" << i << "帧：解码=" << decode_ms << "ms，归一=" << normalize_ms << "ms，render=" << render_ms << "ms，AI=" << ai_ms << "ms，总=" << cost << "ms" << std::endl;
    }
    ai_infer.destroy();
    delete[] model_input; // 释放缓冲区
    decoder.close();
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
//                 std::cout << "输出第" << output_count << "帧（原始第" << frame_count << "帧）" << std::endl;
                // 这里添加你的处理逻辑（如格式转换、输入AI模型）
            }
        }
    }
    
    av_frame_free(&yuv_frame);
    decoder.close();
}

int main() {
    
//    // 先测试阻塞模式（确保完整接收）
//    testBlockPolicy();
//    // 再测试丢弃模式（允许丢旧数据）
//    testDropPolicy();

        avformat_network_init();
    string file_path = "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/data/天鹅.mp4";
        testLocalFile(file_path);
//        testCamera();
    
    return 0;
}


