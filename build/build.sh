
#!/bin/bash

##############################################################################
# 1. 核心配置参数（根据你的目录结构修改，只改这里！）
##############################################################################
# 👉 A 目录路径：CMakeLists.txt 所在的根目录（相对于当前脚本的路径）
# 示例：若脚本在 B 目录，A 是 B 的上级目录 → "../"
CMakeList_DIR="../"

# 👉 构建产物目录名：专门存放 Xcode 项目、编译文件的目录（可自定义，如 build_xcode）
BUILD_DIR_NAME="project"

# 👉 Xcode 项目名：必须和 CMakeLists.txt 中 project(XXX) 的 XXX 一致
XCODE_PROJECT_NAME="mp4_ai_analyzer"

# 👉 编译模式：Debug（调试，默认）/ Release（发布优化）
BUILD_TYPE="Debug"


##############################################################################
# 2. 核心逻辑（无需修改，按配置自动执行）
##############################################################################
# 步骤1：获取脚本所在目录的绝对路径（确保路径不混乱）
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
echo "===== 1. 脚本信息 ====="
echo "脚本所在目录：${SCRIPT_DIR}"
echo "CMake 根目录（CMakeList_DIR 目录）：${SCRIPT_DIR}/${CMakeList_DIR}"
echo "构建产物目录：${SCRIPT_DIR}/${BUILD_DIR_NAME}"

# 步骤2：检查 CMakeList_DIR 目录是否存在 CMakeLists.txt（基础校验）
CMAKE_FILE="${SCRIPT_DIR}/${CMakeList_DIR}/CMakeLists.txt"
echo -e "\n===== 2. 检查 CMakeLists.txt ====="
if [ ! -f "${CMAKE_FILE}" ]; then
    echo "❌ 错误：在 A 目录未找到 CMakeLists.txt！"
    echo "  A 目录路径：${SCRIPT_DIR}/${CMakeList_DIR}"
    echo "  请检查 CMakeList_DIR 配置是否正确！"
    exit 1
fi
echo "✅ 找到 CMakeLists.txt：${CMAKE_FILE}"

# 步骤3：创建/清空独立的 build 目录
BUILD_FULL_DIR="${SCRIPT_DIR}/${BUILD_DIR_NAME}"
echo -e "\n===== 3. 准备构建目录 ====="
# 若 build 目录已存在，先彻底删除
if [ -d "${BUILD_FULL_DIR}" ]; then
    echo "🔄 旧构建目录已存在，正在删除：${BUILD_FULL_DIR}"
    rm -rf "${BUILD_FULL_DIR}"
fi
# 重新创建干净的 build 目录
mkdir -p "${BUILD_FULL_DIR}" || {
    echo "❌ 错误：无法创建 build 目录！"
    echo "  目录路径：${BUILD_FULL_DIR}"
    exit 1
}
echo "✅ 已创建干净的 build 目录：${BUILD_FULL_DIR}"

# 步骤4：进入 build 目录，调用 CMake 生成 Xcode 项目
echo -e "\n===== 4. 生成 Xcode 项目 ====="
cd "${BUILD_FULL_DIR}" || {
    echo "❌ 错误：无法进入 build 目录！"
    echo "  目录路径：${BUILD_FULL_DIR}"
    exit 1
}

# 执行 CMake 命令，指定生成 Xcode 项目
cmake -G Xcode \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      "${SCRIPT_DIR}/${CMakeList_DIR}" || {
    echo -e "\n❌ 错误：CMake 生成 Xcode 项目失败！"
    echo "  请检查：1. CMakeList_DIR 目录 CMake 配置是否正确；2. 第三方库路径是否存在"
    exit 1
}

# 步骤5：验证 Xcode 项目是否生成成功
echo -e "\n===== 5. 验证生成结果 ====="
XCODE_PROJ_PATH="${BUILD_FULL_DIR}/${XCODE_PROJECT_NAME}.xcodeproj"
if [ -d "${XCODE_PROJ_PATH}" ]; then
    echo "✅ Xcode 项目生成成功！"
    echo "  项目路径：${XCODE_PROJ_PATH}"
else
    echo "❌ 错误：未找到生成的 Xcode 项目！"
    echo "  预期路径：${XCODE_PROJ_PATH}"
    exit 1
fi

# 步骤6：自动打开 Xcode 项目（macOS 专属）
echo -e "\n===== 6. 自动打开 Xcode ====="
open "${XCODE_PROJ_PATH}" || {
    echo "⚠️  警告：自动打开 Xcode 失败，可手动双击打开："
    echo "  ${XCODE_PROJ_PATH}"
}

echo -e "\n🎉 所有操作完成！构建产物已集中存放在：${BUILD_FULL_DIR}"
echo "   后续删除/更新产物时，直接操作 ${BUILD_DIR_NAME} 目录即可～"
