LOCAL_PATH := $(call my-dir)

# Syscall number test tool
include $(CLEAR_VARS)
LOCAL_MODULE := syscall_test
LOCAL_SRC_FILES := syscall_test.c
LOCAL_LDFLAGS := -static
include $(BUILD_EXECUTABLE)
