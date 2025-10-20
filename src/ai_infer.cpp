//
//  ai_infer.cpp
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 15/10/2025.
//

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <thread>
#include "ai_infer.h"

//辅助函数： 获取 ONNX 模型的输入/输出节点名称（需要与模型匹配）

void getModelInputOutputNmames(Ort::Session* session,
                               std::vector<std::string>& input_names,
                               std::vector<std::string>& output_names) {
    OrtGetApiBase();
    // 获取输入节点数量（如果使用 MobileNetV2 通常只有1个输入）
    size_t input_count = session->GetInputCount();
    // 获取输出节点数量（通常1个输出：1000类的概率分布）
    size_t output_count = session->GetOutputCount();
    
    // 初始化分配器（用于获取节点名称）
    Ort::AllocatorWithDefaultOptions allocator;
    
    // 获取输入节点名称
    for (size_t i = 0; i < input_count; i++) {
        Ort::AllocatedStringPtr name_ptr = session->GetInputNameAllocated(i, allocator);
        input_names.emplace_back(name_ptr.get());
    }
    
    // 获取输出节点名称
    for (size_t i = 0; i < output_count; i++) {
        Ort::AllocatedStringPtr name_ptr = session->GetOutputNameAllocated(i, allocator);
        output_names.emplace_back(name_ptr.get());
    }
}

// 加载 ImageNet 标签（返回：索引=类别ID，值=类别名称）
std::vector<std::string> load_imagenet_labels(const std::string& file_path) {
    std::vector<std::string> labels;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "[ERROR] 无法打开标签文件: " << file_path << std::endl;
        return labels;  // 返回空向量
    }
    
    std::string line;
    while (std::getline(file, line)) {
        labels.push_back(line);  // 每行一个类别，顺序对应ID 0~999
    }
    file.close();
    return labels;
}



bool AIInfer::init(const std::string &model_path) {
    try {
        //初始化环境（调整日志级别）
        env_ = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "MobileNetV2_Infer");
        // 配置会话选项（启动优化选项，比如算子融合）
        session_options_ = Ort::SessionOptions();
        session_options_.SetGraphOptimizationLevel(ORT_ENABLE_ALL); // 启用算子融合、常量折叠等
        session_options_.SetIntraOpNumThreads(std::thread::hardware_concurrency()); // 自动获取核心数
        session_options_.SetInterOpNumThreads(2); // 跨算子并行线程数
        
        // 加载 ONNX 模型（创建会话）
        session_ = std::make_unique<Ort::Session>(env_,model_path.c_str(),session_options_);
        
        // 获取输入/输出及诶但名称
        getModelInputOutputNmames(session_.get(), input_names_, output_names_);
        std::cout << input_names_.size() << std::endl;
        std::cout << "ONNX模型加载成功，输入节点：" << input_names_[0]
        << "，输出节点：" << output_names_[0] << "\n" << std::endl;
        
        //        imagenet_labels_ = load_imagenet_labels(
        //            "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/imagenet_labels_chinese.txt"
        //        );
        imagenet_labels_ = load_imagenet_labels(
                                                "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/imagenet_labels.txt"
                                                );
        return true;
        
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX模型初始化失败：" << e.what() << std::endl;
        return false;
    }
}

AIResult AIInfer::infer(const float *input_data, int input_size) {
    AIResult result;
    result.is_valid = false;
    
    // 校验参数
    if (!session_ || !input_data || input_size != 224*224*3) {
        std::cerr << "推理参数无效（输入尺寸应为224×224×3）" << std::endl;
        return result;
    }
    
    try {
        // 定义输入形状（NCHW 格式：[batch=1, channel=3,height = 224,width = 224]）
        std::vector<int64_t> input_dims = {1,3,224,224};
        
        //创建输入张量（包装输入数据）
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info,
                                                                  const_cast<float*>(input_data),   // 输入数据指针
                                                                  input_size,                       //数据的长度
                                                                  input_dims.data(),                //输入的形状
                                                                  input_dims.size()                 //形状维度数
                                                                  );
        
        const char* input_name_ptr = input_names_[0].c_str();  // 单个输入名称的指针
        const char* const* input_names_array = &input_name_ptr;  // 指向指针的指针（匹配API要求）
        
        // 2. 准备输出名称数组（同理）
        const char* output_name_ptr = output_names_[0].c_str();
        const char* const* output_names_array = &output_name_ptr;
        
        //执行推理（获取输入张量）
        std::vector<Ort::Value> output_tensors = session_->Run(
                                                               Ort::RunOptions{nullptr},
                                                               input_names_array,  // 取第一个输入节点名称（转为const char*）
                                                               &input_tensor,
                                                               1,  // 输入数量
                                                               output_names_array, // 取第一个输出节点名称（转为const char*）
                                                               1   // 输出数量
                                                               );
        
        //解析输出（MobileNetV2 输出1000类的概率分布）
        float * output_data = output_tensors[0].GetTensorMutableData<float>();
        size_t output_size = output_tensors[0].GetTensorTypeAndShapeInfo().GetElementCount();
        
        // 找到概率最大的类别（Top-1）
        int max_index = 0;
        float max_score = 0.0f;
        for (int i = 0; i < output_size; ++i) {
            if (output_data[i] > max_score) {
                max_score = output_data[i];
                max_index = i;
            }
        }
        std::string class_name;
        if (max_index >= 0 && max_index < imagenet_labels_.size()) {
            class_name = imagenet_labels_[max_index];
        } else {
            class_name = "unknown";  // 无效ID时显示未知
        }
        result.class_name = class_name;  // 临时占位
        result.confidence = max_score;
        result.is_valid = (max_score > 0.5f);  // 置信度>0.5视为有效
        
    } catch (const Ort::Exception e) {
        std::cerr << "推理失败：" << e.what() << std::endl;
    }
    return result;
}

void AIInfer::destroy() {
    // ONNX Runtime的对象会自动析构，无需手动释放
    session_.reset();
}
