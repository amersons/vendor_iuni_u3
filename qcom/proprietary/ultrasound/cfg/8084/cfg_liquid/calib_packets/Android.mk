ifneq ($(BUILD_TINY_ANDROID),true)

ROOT_DIR := $(call my-dir)
LOCAL_PATH := $(ROOT_DIR)
include $(CLEAR_VARS)

# ------------------------------------------------------------------------------
#       Product calib liquid
# ------------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE:= product_calib_liquid.dat
LOCAL_MODULE_CLASS := DATA
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/usf/epos
LOCAL_MODULE_OWNER := qcom
include $(BUILD_PREBUILT)

# ------------------------------------------------------------------------------
#       Unit calib liquid
# ------------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE:= unit_calib_liquid.dat
LOCAL_MODULE_CLASS := DATA
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/usf/epos
LOCAL_MODULE_OWNER := qcom
include $(BUILD_PREBUILT)

# ------------------------------------------------------------------------------
#       Product calib fluid
# ------------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE:= product_calib_fluid.dat
LOCAL_MODULE_CLASS := DATA
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/usf/epos
LOCAL_MODULE_OWNER := qcom
include $(BUILD_PREBUILT)

# ------------------------------------------------------------------------------
#       Unit calib fluid
# ------------------------------------------------------------------------------

include $(CLEAR_VARS)
LOCAL_MODULE:= unit_calib_fluid.dat
LOCAL_MODULE_CLASS := DATA
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/persist/usf/epos
LOCAL_MODULE_OWNER := qcom
include $(BUILD_PREBUILT)

endif #BUILD_TINY_ANDROID

