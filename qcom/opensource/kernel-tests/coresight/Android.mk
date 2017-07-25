BOARD_PLATFORM_LIST := msm8974
BOARD_PLATFORM_LIST += msm8226
BOARD_PLATFORM_LIST += msm8610

ifeq ($(call is-board-platform-in-list,$(BOARD_PLATFORM_LIST)),true)

LOCAL_PATH := $(call my-dir)

define ADD_TEST

include $(CLEAR_VARS)
LOCAL_MODULE       := $1
LOCAL_SRC_FILES    := $1
LOCAL_MODULE_CLASS := EXECUTABLE
LOCAL_MODULE_TAGS  := debug
LOCAL_MODULE_PATH  := $(TARGET_OUT_DATA)/kernel-tests/coresight/core
include $(BUILD_PREBUILT)

endef

TEST_LIST := cs_adversary.sh cs_common.sh cs_test.sh run.sh README.txt
$(foreach TEST,$(TEST_LIST),$(eval $(call ADD_TEST,$(TEST))))

include $(LOCAL_PATH)/*/Android.mk

endif
