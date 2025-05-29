#!/bin/bash

# 设置路径
SDK_ROOT="/home/seb/Workspace/project_camera/OrbbecSDK_v2"
BUILD_DIR="$SDK_ROOT/build"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # 无颜色

echo -e "${GREEN}===== Orbbec 感知系统启动脚本 =====${NC}"

# 检查是否存在 build 目录
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}创建 build 目录...${NC}"
    mkdir -p "$BUILD_DIR"
fi

# 切换到 build 目录
cd "$BUILD_DIR" || { echo -e "${RED}无法切换到 build 目录${NC}"; exit 1; }

# 编译项目
echo -e "${YELLOW}正在编译项目...${NC}"
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j4 demo

# 检查编译结果
if [ $? -ne 0 ]; then
    echo -e "${RED}编译失败，请检查错误信息${NC}"
    exit 1
fi

# 使用 sudo 运行 demo 程序
echo -e "${GREEN}编译成功，使用 sudo 权限启动感知系统...${NC}"
cd "$BUILD_DIR/bin" || { echo -e "${RED}无法切换到 bin 目录${NC}"; exit 1; }
sudo ./demo

# 结束
echo -e "${GREEN}程序已退出${NC}" 