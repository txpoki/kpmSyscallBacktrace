#!/bin/bash
# Test build script

set -e

echo "========================================="
echo "Testing refactored module build"
echo "========================================="

# Set compiler
if [ -z "$TARGET_COMPILE" ]; then
    export TARGET_COMPILE=aarch64-linux-gnu-
fi

echo "Compiler: ${TARGET_COMPILE}gcc"

# Clean
echo ""
echo "Cleaning..."
make clean

# Build
echo ""
echo "Building..."
make

# Check results
echo ""
echo "========================================="
echo "Build Results:"
echo "========================================="

if [ -f "accessOffstinlineHook.kpm" ]; then
    echo "✓ accessOffstinlineHook.kpm"
    ls -lh accessOffstinlineHook.kpm
else
    echo "✗ accessOffstinlineHook.kpm NOT FOUND"
    exit 1
fi

echo ""
echo "Object files:"
for obj in accessOffstinlineHook.o stack_unwind.o process_info.o; do
    if [ -f "$obj" ]; then
        echo "✓ $obj"
        ls -lh $obj
    else
        echo "✗ $obj NOT FOUND"
        exit 1
    fi
done

echo ""
echo "========================================="
echo "Build successful!"
echo "========================================="
echo ""
echo "To deploy: make push"
echo "To load: kpm load /sdcard/kpm/accessOffstinlineHook.kpm"
