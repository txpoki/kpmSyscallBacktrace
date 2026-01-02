#!/system/bin/sh
# 诊断脚本 - 检查模块状态

echo "================================"
echo "KernelPatch/APatch 诊断"
echo "================================"
echo ""

# 1. 检查 KernelPatch 版本
echo "[1] 检查 KernelPatch 版本..."
if [ -f /data/local/tmp/version_test ]; then
    /data/local/tmp/version_test pengxintu123 2>&1 | head -10
else
    echo "  version_test 未找到"
fi
echo ""

# 2. 检查已加载的模块
echo "[2] 检查已加载的模块..."
if command -v kpm >/dev/null 2>&1; then
    echo "  使用 kpm list:"
    kpm list 2>&1 | grep -E "kpm-inline-access|inline|access" || echo "  未找到 kpm-inline-access 模块"
else
    echo "  kpm 命令未找到"
fi
echo ""

# 3. 检查内核日志中的模块信息
echo "[3] 检查内核日志..."
dmesg | grep -i "kpm-inline-access" | tail -5 || echo "  未找到模块日志"
echo ""

# 4. 检查 supercall 是否可用
echo "[4] 检查 supercall..."
cat /proc/kallsyms | grep supercall | head -3 || echo "  未找到 supercall"
echo ""

# 5. 检查 APatch 配置
echo "[5] 检查 APatch 配置..."
if [ -d /data/adb/ap ]; then
    echo "  APatch 目录存在"
    ls -la /data/adb/ap/ 2>&1 | head -10
else
    echo "  APatch 目录不存在"
fi
echo ""

# 6. 检查模块文件
echo "[6] 检查模块文件..."
if [ -d /data/adb/kp/modules ]; then
    echo "  KernelPatch 模块目录:"
    ls -la /data/adb/kp/modules/ 2>&1 | grep -E "\.kpm|inline|access" || echo "  未找到相关模块"
fi
if [ -d /data/adb/ap/modules ]; then
    echo "  APatch 模块目录:"
    ls -la /data/adb/ap/modules/ 2>&1 | grep -E "\.kpm|inline|access" || echo "  未找到相关模块"
fi
echo ""

echo "================================"
echo "诊断完成"
echo "================================"
echo ""
echo "如果模块未加载，请使用以下命令加载:"
echo "  kpm load /path/to/accessOffstinlineHook.kpm"
echo ""
echo "或者检查模块是否在以下位置:"
echo "  /data/adb/kp/modules/"
echo "  /data/adb/ap/modules/"
