ifeq ($(call is-vendor-board-platform,QCOM),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	stm-log.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	system/core/include \
	$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include

LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_CFLAGS += \
	-Wall

# Debug
# LOCAL_CFLAGS += -DDEBUG
# LOCAL_SHARED_LIBRARIES += liblog

LOCAL_MODULE := libstm-log
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        stm-test.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH) \
        system/core/include \
        $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include

LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_CFLAGS += \
        -Wall

LOCAL_SHARED_LIBRARIES := libstm-log
LOCAL_MODULE := test-stm
LOCAL_MODULE_TAGS := optional debug

include $(BUILD_EXECUTABLE)
endif
