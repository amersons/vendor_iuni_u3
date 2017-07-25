ifeq ($(call is-vendor-board-platform,QCOM),true)
ifeq ($(TARGET_ARCH),arm)
LOCAL_PATH := $(call my-dir)
commonSources :=

# the dlkm
DLKM_DIR   := device/qcom/common/dlkm

include $(CLEAR_VARS)
LOCAL_MODULE      := msm_iommu_test_module.ko
LOCAL_MODULE_TAGS := debug
include $(DLKM_DIR)/AndroidKernelModule.mk

# the userspace test program
include $(CLEAR_VARS)
LOCAL_MODULE := msm_iommutest
LOCAL_SRC_FILES += $(commonSources) msm_iommutest.c
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_SHARED_LIBRARIES := \
        libc \
        libcutils \
        libutils
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests/msm_iommu
include $(BUILD_EXECUTABLE)

# the test script
include $(CLEAR_VARS)
LOCAL_MODULE := iommutest.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := iommutest.sh
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests/msm_iommu
include $(BUILD_PREBUILT)

define ADD_TEST

include $(CLEAR_VARS)
LOCAL_MODULE       := $1
LOCAL_SRC_FILES    := $1
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS  := debug
LOCAL_MODULE_PATH  := $(TARGET_OUT_DATA)/kernel-tests/msm_iommu
include $(BUILD_PREBUILT)

endef

ETC_FILE_LIST := msm8974_bfb.txt msm8226_bfb.txt apq8084_bfb.txt apq8084_lpae_bfb.txt mpq8092_bfb.txt README.txt
$(foreach TEST,$(ETC_FILE_LIST),$(eval $(call ADD_TEST,$(TEST))))

endif
endif
