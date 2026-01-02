LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := kpm_control
LOCAL_SRC_FILES := kpm_control.c

# 使用本地的 supercall.h
LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_CFLAGS := -Wall -O2
LOCAL_LDFLAGS := -static

include $(BUILD_EXECUTABLE)
