LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#copy include file to /system/include
LOCAL_COPY_HEADERS_TO := testframework
LOCAL_COPY_HEADERS := inc/testframework.h
LOCAL_COPY_HEADERS += inc/systracer.h
include $(BUILD_COPY_HEADERS)

#testframework lib
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_SRC_FILES := src/Systracer.cpp

LOCAL_SHARED_LIBRARIES := libutils libcutils

ifdef TARGET_USES_TESTFRAMEWORK
LOCAL_CFLAGS := -DCUSTOM_EVENTS_TESTFRAMEWORK

ifeq ($(call is-platform-sdk-version-at-least,18),true)
# JB MR2 or later
LOCAL_CFLAGS += -DJB_MR2=1
endif

LOCAL_C_INCLUDES := $(TOP)/vendor/qcom/opensource/testframework
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include
LOCAL_SRC_FILES += \
        src/TestFrameworkApi.cpp \
        src/TestFrameworkCommon.cpp \
        src/TestFrameworkHash.cpp \
        src/TestFrameworkService.cpp

ifeq ($(TF_FEATURE_USES_BINDER),true)
LOCAL_CFLAGS += -DTF_FEATURE_USE_BINDER
LOCAL_SRC_FILES += src/TestFramework.cpp
LOCAL_SHARED_LIBRARIES += libbinder
endif
endif

ifeq ($(call is-android-codename,JELLY_BEAN),true)
LOCAL_CFLAGS += -DJB
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := qcom
LOCAL_MODULE := libtestframework
include $(BUILD_SHARED_LIBRARY)

#testframework servcice
ifdef TARGET_USES_TESTFRAMEWORK
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
LOCAL_SRC_FILES:= \
        src/TestFrameworkServiceMain.cpp \
	src/TFSShell.cpp

LOCAL_C_INCLUDES := vendor/qcom/opensource/testframework

LOCAL_SHARED_LIBRARIES := libtestframework libcutils libutils

LOCAL_CFLAGS := -DCUSTOM_EVENTS_TESTFRAMEWORK

ifeq ($(TF_FEATURE_USES_BINDER),true)
LOCAL_CFLAGS += -DTF_FEATURE_USE_BINDER
LOCAL_SHARED_LIBRARIES += libbinder
endif

ifeq ($(call is-android-codename,JELLY_BEAN),true)
LOCAL_CFLAGS += -DJB
endif

LOCAL_MODULE_OWNER := qcom
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE := testframeworkservice

include $(BUILD_EXECUTABLE)

endif
