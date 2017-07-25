#Gionee huangzhuolin 20140630 @ add for SRS CR01311718 begin
# SRS Processing
ifeq ($(GN_DTS_SUPPORT), y)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

PRODUCT_COPY_FILES += $(LOCAL_PATH)/srsmodels.lic:system/etc/srs/srsmodels.lic

PRODUCT_COPY_FILES += $(LOCAL_PATH)/srs_processing.cfg:system/etc/srs/srs_processing.cfg
##PRODUCT_COPY_FILES += $(LOCAL_PATH)/srs_processing.cfg:data/data/media/srs_processing.cfg

endif
# SRS Processing
#Gionee huangzhuolin 20140630 @ add for SRS CR01311718 begin
