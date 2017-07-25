ifeq ($(call is-vendor-board-platform,QCOM),true)
ifeq ($(TARGET_ARCH),arm)
LOCAL_PATH := $(call my-dir)

# the dlkm
DLKM_DIR   := device/qcom/common/dlkm

include $(CLEAR_VARS)
LOCAL_MODULE      := the_memory_prof_module.ko
LOCAL_MODULE_TAGS := debug
include $(DLKM_DIR)/AndroidKernelModule.mk

# the userspace test programs
include $(CLEAR_VARS)
LOCAL_MODULE := memory_prof
LOCAL_CFLAGS += -Wno-missing-field-initializers -g
LOCAL_STRIP_MODULE = false
LOCAL_SRC_FILES += memory_prof.c alloc_profiles.c memory_prof_util.c
LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
LOCAL_SHARED_LIBRARIES := \
        libc \
        libcutils \
        libutils
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := memfeast
LOCAL_CFLAGS += -g
LOCAL_STRIP_MODULE = false
LOCAL_SRC_FILES += memfeast.c memory_prof_util.c
LOCAL_SHARED_LIBRARIES := \
        libc \
        libcutils \
        libutils
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_EXECUTABLE)

# the test script
include $(CLEAR_VARS)
LOCAL_MODULE := memory_prof.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := memory_prof.sh
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_PREBUILT)


# the allocation profiles
define ADD_TEST

include $(CLEAR_VARS)
LOCAL_MODULE       := $1
LOCAL_SRC_FILES    := $1
LOCAL_MODULE_CLASS := memory_prof_ETC
LOCAL_MODULE_TAGS  := debug
LOCAL_MODULE_PATH  := $(TARGET_OUT_DATA)/kernel-tests/memory_prof_data
include $(BUILD_PREBUILT)

endef

MEMORY_PROF_ETC_FILE_LIST := README.txt $(subst $(LOCAL_PATH),,$(wildcard $(LOCAL_PATH)/alloc_profiles/*.txt))
$(foreach TEST,$(MEMORY_PROF_ETC_FILE_LIST),$(eval $(call ADD_TEST,$(TEST))))


endif
endif
