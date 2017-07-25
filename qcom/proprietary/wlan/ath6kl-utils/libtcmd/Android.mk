LOCAL_PATH:=$(call my-dir)

# Build libtcmd =========================
include $(CLEAR_VARS)

LOCAL_MODULE := libtcmd
LOCAL_SRC_FILES:= \
    nl80211.c \
    libtcmd.c \
    os.c

ifeq ($(BOARD_HAS_ATH_WLAN_AR6004),true)
	LOCAL_CFLAGS+= -DCONFIG_AR6002_REV6
endif

LOCAL_CFLAGS += -include bionic/libc/kernel/arch-arm/asm/posix_types.h
LOCAL_CFLAGS += -include bionic/libc/kernel/arch-arm/asm/byteorder.h
LOCAL_CFLAGS += -include bionic/libc/kernel/common/linux/posix_types.h
LOCAL_CFLAGS += -include bionic/libc/kernel/common/linux/types.h
LOCAL_CFLAGS += -include bionic/libc/kernel/common/linux/socket.h

LOCAL_C_INCLUDES += external/libnl-headers
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_COPY_HEADERS_TO := libtcmd
LOCAL_COPY_HEADERS := libtcmd.h

LOCAL_CFLAGS += \
	-DWLAN_API_NL80211 \
	-DANDROID \
	-DLIBNL_2 \

# ICS ships with libnl 2.0
LOCAL_SHARED_LIBRARIES := libnl_2

LOCAL_MODULE_OWNER := qcom

include $(BUILD_STATIC_LIBRARY)
