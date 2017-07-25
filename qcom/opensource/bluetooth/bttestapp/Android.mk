ifneq ("$(APK_DEFAULT_BTTESTAPP_SUPPORT)", "no")

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
        $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := BTTestApp
LOCAL_CERTIFICATE := platform

LOCAL_MODULE_OWNER := qcom

LOCAL_STATIC_JAVA_LIBRARIES := com.android.vcard org.codeaurora.bluetooth.mapclient org.codeaurora.bluetooth.pbapclient

LOCAL_PROGUARD_ENABLED := disabled

include $(BUILD_PACKAGE)

include $(CLEAR_VARS)

src_dirs:=  ../src/org/codeaurora/bluetooth/mapclient

LOCAL_SRC_FILES := \
        $(call all-java-files-under, $(src_dirs))

LOCAL_MODULE:= org.codeaurora.bluetooth.mapclient
LOCAL_JAVA_LIBRARIES := javax.obex
LOCAL_STATIC_JAVA_LIBRARIES := com.android.vcard

LOCAL_PROGUARD_ENABLED := disabled

include $(BUILD_STATIC_JAVA_LIBRARY)

include $(CLEAR_VARS)

src_dirs:=  ../src/org/codeaurora/bluetooth/pbapclient

LOCAL_SRC_FILES := \
        $(call all-java-files-under, $(src_dirs))

LOCAL_MODULE:= org.codeaurora.bluetooth.pbapclient
LOCAL_JAVA_LIBRARIES := javax.obex
LOCAL_STATIC_JAVA_LIBRARIES := com.android.vcard

LOCAL_PROGUARD_ENABLED := disabled

include $(BUILD_STATIC_JAVA_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))

endif
