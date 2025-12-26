MODULE_NAME := accessOffstinlineHook
OBJS := $(MODULE_NAME).o
TARGET_COMPILE = aarch64-linux-gnu-
ifndef TARGET_COMPILE
$(error TARGET_COMPILE not set)
endif

ifndef KP_DIR
KP_DIR = ../..
endif

CC = $(TARGET_COMPILE)gcc
LD = $(TARGET_COMPILE)ld

# 核心编译选项
CFLAGS += -O2 -g \
          -D__KERNEL__ -DMODULE \
          -fno-stack-protector -fno-pic -fno-plt -mcmodel=large \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -fno-strict-aliasing \
          -Wno-declaration-after-statement \
          -Wno-unused-function -Wno-unused-variable

# 头文件路径
INCLUDE_DIRS := . \
                include \
                patch/include \
                linux/include \
                linux/arch/arm64/include \
                linux/tools/arch/arm64/include

INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(KP_DIR)/kernel/$(dir))

all: $(MODULE_NAME).kpm

$(MODULE_NAME).kpm: $(OBJS)
	$(CC) -r -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -c -o $@ $<

.PHONY: clean push
clean:
	rm -rf *.kpm *.o *.mod.c .*.cmd

push: $(MODULE_NAME).kpm
	adb push $(MODULE_NAME).kpm /sdcard/kpm