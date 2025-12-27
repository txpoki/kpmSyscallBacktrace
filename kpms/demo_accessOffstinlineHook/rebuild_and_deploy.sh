#!/bin/bash

# 重新编译和部署脚本

set -e

echo "=========================================="
echo "1. 清理旧的编译文件"
echo "=========================================="
make clean

echo ""
echo "=========================================="
echo "2. 重新编译模块"
echo "=========================================="
make

echo ""
echo "=========================================="
echo "3. 检查编译结果"
echo "=========================================="
if [ -f "accessOffstinlineHook.kpm" ]; then
    echo "✓ 编译成功: accessOffstinlineHook.kpm"
    ls -lh accessOffstinlineHook.kpm
else
    echo "✗ 编译失败: 找不到 accessOffstinlineHook.kpm"
    exit 1
fi

echo ""
echo "=========================================="
echo "4. 部署到设备"
echo "=========================================="
echo "请手动执行以下命令将模块部署到设备："
echo ""
echo "  # 1. 推送到设备"
echo "  adb push accessOffstinlineHook.kpm /data/local/tmp/"
echo ""
echo "  # 2. 卸载旧模块（如果已加载）"
echo "  adb shell su -c 'kpm unload accessOffstinlineHook'"
echo ""
echo "  # 3. 加载新模块"
echo "  adb shell su -c 'kpm load /data/local/tmp/accessOffstinlineHook.kpm'"
echo ""
echo "  # 4. 查看日志"
echo "  adb shell su -c 'dmesg | grep MyHook'"
echo ""
echo "=========================================="
echo "完成！"
echo "=========================================="
