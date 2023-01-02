LOCAL_PATH := $(call my-dir)

ifneq ($(filter yes,$(sort $(MTK_WLAN_SUPPORT) $(MTK_BT_SUPPORT) $(MTK_GPS_SUPPORT) $(MTK_FM_SUPPORT))),)

ifneq ($(MTK_COMBO_CHIP),MT7668)

ifneq (true,$(strip $(TARGET_NO_KERNEL)))
ifneq ($(filter yes,$(MTK_COMBO_SUPPORT)),)

include $(CLEAR_VARS)
LOCAL_MODULE := wmt_drv.ko
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_INIT_RC := init.wmt_drv.rc
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(shell find $(LOCAL_PATH) -type f -name '*.[cho]')) Makefile
LOCAL_REQUIRED_MODULES :=

include $(MTK_KERNEL_MODULE)

else
        $(warning wmt_drv-MTK_COMBO_SUPPORT: [$(MTK_COMBO_SUPPORT)])
endif
endif
else
        $(warning skip wmt_drv for $(MTK_COMBO_CHIP))
endif

endif
