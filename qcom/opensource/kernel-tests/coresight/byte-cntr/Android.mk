LOCAL_PATH := $(call my-dir)

define ADD_TEST

include $(CLEAR_VARS)
LOCAL_MODULE       := $1
LOCAL_SRC_FILES    := $1
LOCAL_MODULE_CLASS := EXECUTABLE
LOCAL_MODULE_TAGS  := debug
LOCAL_MODULE_PATH  := $(TARGET_OUT_DATA)/kernel-tests/coresight/core/byte-cntr
include $(BUILD_PREBUILT)

endef

TEST_LIST := byte_cntr.sh
$(foreach TEST,$(TEST_LIST),$(eval $(call ADD_TEST,$(TEST))))
