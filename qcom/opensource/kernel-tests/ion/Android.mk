ifeq ($(call is-vendor-board-platform,QCOM),true)
ifeq ($(TARGET_ARCH),arm)
LOCAL_PATH := $(call my-dir)
commonSources :=

# the dlkm
DLKM_DIR   := device/qcom/common/dlkm

include $(CLEAR_VARS)
LOCAL_MODULE      := msm_ion_test_module.ko
LOCAL_MODULE_TAGS := debug
include $(DLKM_DIR)/AndroidKernelModule.mk

# the userspace test program
include $(CLEAR_VARS)
LOCAL_MODULE := msm_iontest
LOCAL_SRC_FILES += $(commonSources) msm_iontest.c kernel_ion_tests.c user_ion_tests.c cp_ion_tests.c
LOCAL_SRC_FILES += ion_test_utils.c
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_SHARED_LIBRARIES := \
        libc \
        libcutils \
        libutils
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_EXECUTABLE)

# the test script
include $(CLEAR_VARS)
LOCAL_MODULE := iontest.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := iontest.sh
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_PREBUILT)

endif
endif
