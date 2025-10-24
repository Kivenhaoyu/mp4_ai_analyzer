cmake_minimum_required(VERSION 3.5.0)
project(mp4_ai_analyzer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成Xcode项目时保留框架链接信息（解决Foundation等框架找不到的问题）
set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
set(CMAKE_XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "/System/Library/Frameworks")

# 配置ONNX Runtime（关键修复1：路径和库名修正）
set(ONNX_INSTALL_PATH "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/onnxruntime")
# 头文件路径（正确，include目录下是onnxruntime的头文件）
include_directories(${ONNX_INSTALL_PATH}/include)
# 库文件路径（正确，lib目录下存放dylib）
link_directories(${ONNX_INSTALL_PATH}/lib)
# 库名修正：CMake链接时不需要前缀"lib"和后缀".dylib"，直接写"onnxruntime.1.14.1"
set(ONNX_LIBS onnxruntime.1.23.1)

# 配置FFmpeg
set(FFMPEG_INSTALL_PATH "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/ffmpeg_install")
include_directories(${FFMPEG_INSTALL_PATH}/include)
link_directories(${FFMPEG_INSTALL_PATH}/lib)
set(FFMPEG_LIBS avutil avcodec avformat swscale avdevice)

# 配置OpenCV
set(OPENCV_INSTALL_PATH "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/opencv-install")
include_directories(${OPENCV_INSTALL_PATH}/include/opencv4)
link_directories(${OPENCV_INSTALL_PATH}/lib)
set(OPENCV_LIBS opencv_core opencv_highgui opencv_imgproc opencv_imgcodecs)

# 收集源文件
file(GLOB_RECURSE ALL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/src/*.h"
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# 创建逻辑分组（与实际路径一致，避免冗余）
foreach(FILE ${ALL_SOURCE_FILES})
    # 基于src目录计算相对路径（确保分组从src开始）
    file(RELATIVE_PATH RELATIVE_FILE_PATH "${PROJECT_SOURCE_DIR}/src" "${FILE}")
    get_filename_component(FILE_DIR "${RELATIVE_FILE_PATH}" DIRECTORY)
    string(REPLACE "/" "\\" GROUP_NAME "src\\${FILE_DIR}")  # 分组以src为根
    source_group("${GROUP_NAME}" FILES "${FILE}")
endforeach()


# 创建目标
add_executable(${PROJECT_NAME} ${ALL_SOURCE_FILES})


# 链接依赖
target_link_libraries(
    ${PROJECT_NAME}
    ${FFMPEG_LIBS}
    ${OPENCV_LIBS}
    ${ONNX_LIBS} 
    # 系统框架
    "-framework Foundation"
    "-framework AVFoundation"
    "-framework CoreMedia"
    "-framework CoreGraphics"
    "-framework CoreVideo"
    "-framework VideoToolbox"
    z
    iconv
    pthread
    # 系统库
)
