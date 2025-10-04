cmake_minimum_required(VERSION 3.5.0)
project(mp4_ai_analyzer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 配置FFmpeg
set(FFMPEG_INSTALL_PATH "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/ffmpeg_install")
include_directories(${FFMPEG_INSTALL_PATH}/include)
link_directories(${FFMPEG_INSTALL_PATH}/lib)
set(FFMPEG_LIBS avutil avcodec avformat swscale)

# 2. 配置OpenCV
set(OPENCV_INSTALL_PATH "/Users/elenahao/AaronWorkFiles/Ocean/mp4_ai_analyzer/lib/opencv-install")
include_directories(${OPENCV_INSTALL_PATH}/include/opencv4)
link_directories(${OPENCV_INSTALL_PATH}/lib)
set(OPENCV_LIBS opencv_core opencv_highgui opencv_imgproc opencv_imgcodecs)

# 3. 收集源文件（.h/.cpp）
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

# 1. 找到 macOS 系统框架（CoreFoundation、CoreVideo）
find_library(CORE_FOUNDATION CoreFoundation)
find_library(CORE_VIDEO CoreVideo)

# 2. 找到 iconv 库（macOS 系统自带，无需额外安装）
find_library(ICONV_LIB iconv)
find_library(Z_LIB z)

# 5. 链接依赖
target_link_libraries(
    ${PROJECT_NAME}
    ${FFMPEG_LIBS}
    ${OPENCV_LIBS}
    ${CORE_VIDEO}
    ${CORE_FOUNDATION}
    ${Z_LIB} # libz.tbd（压缩库）
    ${ICONV_LIB} #libiconv.tbd（字符编码转换库）
    pthread
)
