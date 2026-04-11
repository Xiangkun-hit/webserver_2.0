# rm -rf build
# mkdir build && cd build
# cmake ..

#!/bin/bash

# 自动获取项目根目录（不用手动修改路径）
PROJECT_DIR=$(cd "$(dirname "$0")" && pwd)

# 1. 彻底清理旧编译缓存（必须做，避免旧配置干扰）
echo ">>> 1/4 清理旧build文件夹..."
rm -rf "${PROJECT_DIR}/build"

# 2. 创建build目录并进入
echo ">>> 2/4 创建build目录..."
mkdir -p "${PROJECT_DIR}/build" && cd "${PROJECT_DIR}/build"

# 3. 执行CMake生成Makefile
echo ">>> 3/4 执行CMake配置..."
cmake ..

# 4. 并行编译（用所有CPU核心，速度翻倍）
echo ">>> 4/4 开始编译..."
make -j$(nproc)

# 5. 检查编译结果
if [ $? -eq 0 ]; then
    echo -e "\n✅ 编译成功！"
    echo "📦 可执行文件路径：${PROJECT_DIR}/build/server"
    # 可选：编译成功后自动启动服务器
    read -p "是否立即启动服务器？(y/n) " ans
    if [ "$ans" = "y" ]; then
        echo ">>> 启动Web服务器..."
        cd "${PROJECT_DIR}/build" && ./server
    fi
else
    echo -e "\n❌ 编译失败！请检查上方错误信息"
    exit 1
fi