LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Logging Features. Enable only one at any time
#LOCAL_CFLAGS += -DFEATURE_DATA_LOG_STDERR
#LOCAL_CFLAGS += -DFEATURE_DATA_LOG_SYSLOG
#LOCAL_CFLAGS += -DFEATURE_DATA_LOG_ADB
LOCAL_CFLAGS += -DFEATURE_DATA_LOG_QXDM

LOCAL_SRC_FILES := qdi.c \
                   qdi_netlink.c

LOCAL_SHARED_LIBRARIES := libqmi
LOCAL_SHARED_LIBRARIES += libdiag
LOCAL_SHARED_LIBRARIES += libdsutils

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../inc
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/diag/include
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/qmi/inc
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/data/inc
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/common/inc

LOCAL_MODULE := libqdi
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_OWNER := qcom
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
