ifeq ($(call is-vendor-board-platform,QCOM),true)
ifeq ($(TARGET_ARCH),arm)

DLKM_DIR := device/qcom/common/dlkm
LOCAL_PATH := $(call my-dir)

# the dlkm
include $(CLEAR_VARS)
LOCAL_MODULE      := msm_ocmem_test_module.ko
LOCAL_MODULE_TAGS := debug
include $(DLKM_DIR)/AndroidKernelModule.mk

# the userspace test program
include $(CLEAR_VARS)
LOCAL_MODULE      := ocmem_test
LOCAL_SRC_FILES   := ocmem_test.c
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_EXECUTABLE)

# the test script
include $(CLEAR_VARS)
LOCAL_MODULE       := ocmem_test.sh
LOCAL_SRC_FILES    := ocmem_test.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS  := debug
LOCAL_MODULE_PATH  := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_PREBUILT)

endif
endif
