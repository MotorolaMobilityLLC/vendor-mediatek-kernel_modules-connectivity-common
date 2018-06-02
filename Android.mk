LOCAL_PATH := $(call my-dir)

ifneq ($(filter yes,$(sort $(MTK_WLAN_SUPPORT) $(MTK_BT_SUPPORT) $(MTK_GPS_SUPPORT) $(MTK_FM_SUPPORT))),)

ifeq ($(LINUX_KERNEL_VERSION),)
    $(error LINUX_KERNEL_VERSION is not defined)
endif

KERNEL_ENV_PATH := $(LINUX_KERNEL_VERSION)
ifndef KERNEL_ROOT_DIR
KERNEL_ROOT_DIR := $(PWD)
endif#KERNEL_ZIMAGE_OUT

ifneq (true,$(strip $(TARGET_NO_KERNEL)))
ifndef KERNEL_DIR
  KERNEL_DIR := $(KERNEL_ENV_PATH)
endif#KERNEL_DIR

  ifeq ($(KERNEL_CROSS_COMPILE),)
    ifeq ($(TARGET_ARCH), arm64)
      KERNEL_CROSS_COMPILE := $(KERNEL_ROOT_DIR)/$(TARGET_TOOLS_PREFIX)
    else
      KERNEL_CROSS_COMPILE := $(KERNEL_ROOT_DIR)/prebuilts/gcc/$(HOST_PREBUILT_TAG)/arm/arm-eabi-$(TARGET_GCC_VERSION)/bin/arm-eabi-
    endif
  endif

    KERNEL_OUT ?= $(if $(filter /% ~%,$(TARGET_OUT_INTERMEDIATES)),,$(KERNEL_ROOT_DIR)/)$(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
  ifndef KERNEL_ZIMAGE_OUT
    ifeq ($(TARGET_ARCH), arm64)
      ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/Image.gz-dtb
      else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/Image.gz
      endif
    else
      ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/zImage-dtb
      else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(TARGET_ARCH)/boot/zImage
      endif
    endif
  endif#KERNEL_ZIMAGE_OUT

  ifndef KERNEL_MAKE_OPTION
    KERNEL_MAKE_OPTION := O=$(KERNEL_OUT) ARCH=$(TARGET_ARCH) CROSS_COMPILE=$(KERNEL_CROSS_COMPILE) ROOTDIR=$(KERNEL_ROOT_DIR) $(if $(strip $(SHOW_COMMANDS)),V=1)
  endif

# check kernel folder exist
ifeq (,$(wildcard $(KERNEL_DIR)))
    $(error kernel $(KERNEL_DIR) is not existed)
endif

# check cross compiler exist
ifeq (,$(wildcard $(KERNEL_CROSS_COMPILE)gcc))
    $(error $(KERNEL_CROSS_COMPILE) is not existed)
endif

# set for module Makefile
export AUTOCONF_H=$(KERNEL_OUT)/include/generated/autoconf.h

ifneq ($(filter yes,$(MTK_COMBO_SUPPORT)),)

include $(CLEAR_VARS)
LOCAL_MODULE := wmt_drv.ko
LOCAL_STRIP_MODULE := true
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := first
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib/modules
LOCAL_INIT_RC := init.wmt_drv.rc
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(shell find $(LOCAL_PATH) -type f -name '*.[cho]')) Makefile
LOCAL_REQUIRED_MODULES :=

include $(BUILD_SYSTEM)/base_rules.mk

LOCAL_GENERATED_SOURCES := $(addprefix $(intermediates)/,$(LOCAL_SRC_FILES))

$(LOCAL_GENERATED_SOURCES): $(intermediates)/% : $(LOCAL_PATH)/% | $(ACP)
	$(copy-file-to-target)

$(KERNEL_OUT)/scripts/sign-file: $(KERNEL_ZIMAGE_OUT);

$(LOCAL_BUILT_MODULE): KOUT := $(KERNEL_OUT)
$(LOCAL_BUILT_MODULE): KOPTS := $(KERNEL_MAKE_OPTION) M=$(abspath $(intermediates))
$(LOCAL_BUILT_MODULE): $(LOCAL_GENERATED_SOURCES) $(KERNEL_ZIMAGE_OUT)
$(LOCAL_BUILT_MODULE): CERT_PATH := $(LINUX_KERNEL_VERSION)/certs
$(LOCAL_BUILT_MODULE): $(wildcard $(LINUX_KERNEL_VERSION)/certs/ko_prvk.pem)
$(LOCAL_BUILT_MODULE): $(wildcard $(LINUX_KERNEL_VERSION)/certs/ko_pubk.x509.der)
$(LOCAL_BUILT_MODULE): $(wildcard vendor/mediatek/proprietary/scripts/kernel_tool/rm_ko_sig.py)
$(LOCAL_BUILT_MODULE): $(KERNEL_OUT)/scripts/sign-file
	@echo $@: $^
	$(MAKE) -C $(KOUT) $(KOPTS)
	$(hide) $(call sign-kernel-module,$(KOUT)/scripts/sign-file,$(CERT_PATH)/ko_prvk.pem,$(CERT_PATH)/ko_pubk.x509.der)
else
        $(warning wmt_drv-MTK_COMBO_SUPPORT: [$(MTK_COMBO_SUPPORT)])
endif
endif
endif
