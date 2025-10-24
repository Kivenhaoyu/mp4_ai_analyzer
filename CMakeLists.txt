cmake_minimum_required(VERSION 3.5.0)
project(mp4_ai_analyzer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (APPLE)
   set(PLATFORM_MAC TRUE)
   message(STATUS "构建 Mac 平台版本")
elseif(UNIX AND NOT APPLE) #Linux
   set(PLATFORM_LINUX TRUE)
   message(STATUS "构建 Linux 平台版本")
elseif(WIN32)
    set(PLATFORM_WIN TRUE)
    message(STATUS "构建Windows平台版本")
else()
    message(FATAL_ERROR "不支持的平台")
endif()

set(ONNX_INSTALL_PATH "" CACHE PATH "ONNX Runtime安装路径")
set(FFMPEG_INSTALL_PATH "" CACHE PATH "FFmpeg安装路径")
set(OPENCV_INSTALL_PATH "" CACHE PATH "OpenCV安装路径")

if(NOT ONNX_INSTALL_PATH)
    message(FATAL_ERROR "请通过-DONNX_INSTALL_PATH指定ONNX Runtime路径")
endif()
if(NOT FFMPEG_INSTALL_PATH)
    message(FATAL_ERROR "请通过-DFFMPEG_INSTALL_PATH指定 FFmpeg 路径")
endif()
if(NOT OPENCV_INSTALL_PATH)
    message(FATAL_ERROR "请通过-DOPENCV_INSTALL_PATH指定 OpenCV 路径")
endif()

# -------------------------- 依赖配置（跨平台处理） --------------------------
# 配置ONNX Runtime（关键修复1：路径和库名修正）
# set(ONNX_INSTALL_PATH "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/onnxruntime")
# 头文件路径（正确，include目录下是onnxruntime的头文件）
include_directories(${ONNX_INSTALL_PATH}/include)
# 库文件路径（正确，lib目录下存放dylib）
if (PLATFORM_MAC)
    link_directories(${ONNX_INSTALL_PATH}/lib/mac)
else()
    link_directories(${ONNX_INSTALL_PATH}/lib)
endif()
# 库名修正：CMake链接时不需要前缀"lib"和后缀".dylib"，直接写"onnxruntime.1.14.1"
set(ONNX_LIBS onnxruntime)

# 配置FFmpeg
# set(FFMPEG_INSTALL_PATH "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/ffmpeg_install")
include_directories(${FFMPEG_INSTALL_PATH}/include)
if (PLATFORM_MAC)
    link_directories(${FFMPEG_INSTALL_PATH}/lib/mac)
else()
    link_directories(${FFMPEG_INSTALL_PATH}/lib)
endif()
set(FFMPEG_LIBS avutil avcodec avformat swscale avdevice)

# 配置OpenCV
# set(OPENCV_INSTALL_PATH "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/opencv-install")
include_directories(${OPENCV_INSTALL_PATH}/include/opencv4)
if (PLATFORM_MAC)
    link_directories(${OPENCV_INSTALL_PATH}/lib/mac)
else()
    link_directories(${OPENCV_INSTALL_PATH}/lib)
endif()

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
    if(WIN32)
        string(REPLACE "/" "\\" GROUP_NAME "src\\${FILE_DIR}")  # Windows路径分隔符
    else()
        set(GROUP_NAME "src/${FILE_DIR}")   # Mac/Linux用/
    endif()
    source_group("${GROUP_NAME}" FILES "${FILE}")
endforeach()

# 创建目标
add_executable(${PROJECT_NAME} ${ALL_SOURCE_FILES})

# 链接通用库
target_link_libraries(${PROJECT_NAME}
    ${FFMPEG_LIBS}
    ${OPENCV_LIBS}
    ${ONNX_LIBS}
    pthread  # 跨平台线程库
    z        # 压缩库（跨平台）
)

if(PLATFORM_MAC)
    # 生成Xcode项目时保留框架链接信息（解决Foundation等框架找不到的问题）
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
    set(CMAKE_XCODE_ATTRIBUTE_FRAMEWORK_SEARCH_PATHS "/System/Library/Frameworks")
    # 链接依赖
    target_link_libraries(
        ${PROJECT_NAME}
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreMedia"
        "-framework CoreGraphics"
        "-framework CoreVideo"
        "-framework VideoToolbox"
        iconv
    )
elseif(PLATFORM_LINUX)
    target_link_libraries(${PROJECT_NAME}
        dl       # 动态链接库支持
        rt       # 实时库（部分Linux系统需要）
    )
elseif(PLATFORM_WIN32)
    # Windows特有库（未来扩展）
    target_link_libraries(${PROJECT_NAME}
        ws2_32   # Windows网络库（如果需要）
    )
endif()



