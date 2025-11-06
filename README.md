mp4_ai_analyzer

src/
├── core/                  # 播放器核心控制（状态管理、音画同步）
│   ├── player.h           # 播放器接口（播放/暂停/快进等控制）
│   ├── player.cpp         # 播放器实现（状态机、同步逻辑）
│   ├── media_state.h      # 媒体状态定义（播放状态、进度等）
│   └── sync_manager.h     # 音画同步管理器（时间戳对齐逻辑）
│   └── sync_manager.cpp   
│
├── decoder/               # 音视频解码（拆分原video_decoder，独立音频处理）
│   ├── video_decoder.h    # 视频解码器接口
│   ├── video_decoder.cpp  # 视频解码实现（保留原FFmpeg逻辑）
│   ├── audio_decoder.h    # 音频解码器接口
│   ├── audio_decoder.cpp  # 音频解码实现（拆分原注释中的音频代码）
│   └── codec_utils.h      # 编解码器工具（共享的FFmpeg辅助函数）
│
├── ai/                    # AI推理模块（原ai_infer，更聚焦）
│   ├── infer_engine.h     # AI推理接口（支持多模型扩展）
│   ├── infer_engine.cpp   # 推理实现（封装ONNX Runtime）
│   └── model_loader.h     # 模型加载器（独立处理模型路径、标签）
│   └── model_loader.cpp
│
├── render/                # 渲染模块（整合原render相关）
│   ├── base_renderer.h    # 渲染器基类（抽象接口）
│   ├── video_renderer.h   # 视频渲染接口
│   ├── platform/          # 平台相关实现
│   │   ├── mac_video_renderer.h/cpp
│   │   ├── win_video_renderer.h/cpp
│   │   └── linux_video_renderer.h/cpp
│
├── audio/                 # 音频输出模块（新增，支持跨平台播放）
│   ├── audio_player.h     # 音频播放器接口
│   ├── platform/
│   │   ├── mac_audio_player.h/cpp  # 基于CoreAudio
│   │   ├── win_audio_player.h/cpp  # 基于DirectSound
│   │   └── linux_audio_player.h/cpp # 基于ALSA
│
├── util/                  # 工具模块（整合原util）
│   ├── frame_pool.h       # 帧池管理（视频帧、音频帧）
│   ├── frame_pool.cpp
│   ├── frame_guard.h      # 帧智能管理（保持不变）
│   ├── time_utils.h       # 时间戳转换工具（音画同步用）
│   └── time_utils.cpp
│
├── common/                # 公共基础（共享数据结构、日志等）
│   ├── log/               # 日志模块（保持不变）
│   ├── data_structs.h     # 共享数据结构（帧数据、AI结果等）
│   └── platform.h         # 平台宏定义（保持不变）
│
├── config/                # 配置模块（新增，管理路径、参数）
│   ├── config.h           # 配置接口（模型路径、窗口大小等）
│   └── config.cpp         # 配置加载（从文件或默认值）
│
├── test/                  # 测试模块（拆分原main中的test函数）
│   ├── test_camera.cpp    # 摄像头测试
│   ├── test_local_file.cpp # 本地文件测试
│   └── test_ai.cpp        # AI推理单独测试
│
└── main.cpp               # 程序入口（仅初始化+启动播放器）
