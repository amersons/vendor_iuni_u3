BOARD_PLATFORM_LIST := msm8660
BOARD_PLATFORM_LIST += msm8960
BOARD_PLATFORM_LIST += apq8064
BOARD_PLATFORM_LIST += msm8930
BOARD_PLATFORM_LIST += msm8974
BOARD_PLATFORM_LIST += mdm9625
BOARD_PLATFORM_LIST += msm8226
BOARD_PLATFORM_LIST += msm8610

ifeq ($(call is-board-platform-in-list, $(BOARD_PLATFORM_LIST)),true)

DLKM_DIR   := device/qcom/common/dlkm
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_COPY_HEADERS_TO   := kernel-tests/msm_bus
LOCAL_COPY_HEADERS      := msm_bus_test.h
include $(BUILD_COPY_HEADERS)

LOCAL_MODULE      := msm_bus_ioctl.ko
LOCAL_MODULE_TAGS := eng
include $(DLKM_DIR)/AndroidKernelModule.mk

include $(CLEAR_VARS)
LOCAL_MODULE      := msm_bus_test
LOCAL_SRC_FILES   := msm_bus_test.c
LOCAL_C_FLAGS := -lpthread
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_EXECUTABLE)

endif
