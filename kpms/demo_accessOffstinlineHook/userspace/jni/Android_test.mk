LOCAL_PATH := $(call my-dir)

# Version test tool
include $(CLEAR_VARS)
LOCAL_MODULE := version_test
LOCAL_SRC_FILES := version_test.c
LOCAL_LDFLAGS := -static
include $(BUILD_EXECUTABLE)
