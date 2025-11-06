//
//  infer_engine.h
//  mp4_ai_analyzer
//
//  Created by Elena Aaron on 15/10/2025.
//

#ifndef INFER_ENGINE_H
#define INFER_ENGINE_H

#include <string>
#include <vector>

#include "onnxruntime_cxx_api.h"
#include "onnxruntime_c_api.h"

struct AIResult {
    std::string class_name;     //类别名称（如：“水杯”）
    float confidence;           //可信度（0-1）
    bool is_valid;              //结果是否有效（避免类型报错）
};

//推理类
class AIInfer {
public:
    //加载模型
    bool init(const std::string& model_path);
    
    //开始推理：输入归一化结果，输出结果
    AIResult infer(const float * input_data, int input_size);
        
    //销毁资源
    void destroy();
    
private:
    // ONNX Runtime 核心对象
    Ort::Env env_;                              //环境对象（全局唯一）
    Ort::SessionOptions session_options_;       //会话配置（优化级别，线程数）
    std::unique_ptr<Ort::Session> session_;     //推理会话
    std::vector<std::string> input_names_;      //输入节点名称
    std::vector<std::string> output_names_;     //输出节点名称
    
    std::vector<std::string> imagenet_labels_;  //需要查的对应的表
};


#endif /* INFER_ENGINE_H */
