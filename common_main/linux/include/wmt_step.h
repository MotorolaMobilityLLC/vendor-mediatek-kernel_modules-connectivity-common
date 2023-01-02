/*
 * Copyright (C) 2016 MediaTek Inc. *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _WMT_STEP_H_
#define _WMT_STEP_H_

#include <linux/list.h>

#include "osal.h"
#include "wmt_exp.h"
#include "wmt_core.h"

#define STEP_CONFIG_NAME "WMT_STEP.cfg"

#define STEP_PERIODIC_DUMP_WORK_QUEUE "wmt_step_pd_wq"
#define STEP_PERIODIC_DUMP_THREAD "wmt_pd"

#define STEP_ACTION_NAME_EMI "_EMI"
#define STEP_ACTION_NAME_REGISTER "_REG"
#define STEP_ACTION_NAME_GPIO "GPIO"
#define STEP_ACTION_NAME_DISABLE_RESET "DRST"
#define STEP_ACTION_NAME_CHIP_RESET "_RST"
#define STEP_ACTION_NAME_KEEP_WAKEUP "WAK+"
#define STEP_ACTION_NAME_CANCEL_WAKEUP "WAK-"

extern struct platform_device *g_pdev;

enum step_action_id {
	STEP_ACTION_INDEX_NO_DEFINE = 0,
	STEP_ACTION_INDEX_EMI = 1,
	STEP_ACTION_INDEX_REGISTER,
	STEP_ACTION_INDEX_GPIO,
	STEP_ACTION_INDEX_DISABLE_RESET,
	STEP_ACTION_INDEX_CHIP_RESET,
	STEP_ACTION_INDEX_KEEP_WAKEUP,
	STEP_ACTION_INDEX_CANCEL_WAKEUP,
	STEP_ACTION_INDEX_PERIODIC_DUMP,
	STEP_ACTION_INDEX_MAX,
};

enum step_trigger_point_id {
	STEP_TRIGGER_POINT_NO_DEFINE = 0,
	STEP_TRIGGER_POINT_COMMAND_TIMEOUT = 1,
	STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT,
	STEP_TRIGGER_POINT_BEFORE_CHIP_RESET,
	STEP_TRIGGER_POINT_AFTER_CHIP_RESET,
	STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_OFF,
	STEP_TRIGGER_POINT_BEFORE_BT_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_BT_FUNC_OFF,
	STEP_TRIGGER_POINT_BEFORE_FM_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_FM_FUNC_OFF,
	STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_ON,
	STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_OFF,
	STEP_TRIGGER_POINT_BEFORE_READ_THERMAL,
	STEP_TRIGGER_POINT_POWER_ON_START,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_GET_CONNSYS_ID,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_SEND_DOWNLOAD_PATCH,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_CONNSYS_RESET,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_SET_WIFI_LTE_COEX,
	STEP_TRIGGER_POINT_POWER_ON_BEFORE_BT_WIFI_CALIBRATION,
	STEP_TRIGGER_POINT_POWER_ON_END,
	STEP_TRIGGER_POINT_BEFORE_POWER_OFF,
	STEP_TRIGGER_POINT_WHEN_AP_SUSPEND,
	STEP_TRIGGER_POINT_WHEN_AP_RESUME,
	STEP_TRIGGER_POINT_MAX,
};

enum step_register_base_id {
	STEP_REGISTER_PHYSICAL_ADDRESS = 0,
	STEP_REGISTER_CONN_MCU_CONFIG_BASE,
	STEP_REGISTER_AP_RGU_BASE,
	STEP_REGISTER_TOPCKGEN_BASE,
	STEP_REGISTER_SPM_BASE,
	STEP_REGISTER_HIF_ON_BASE,
	STEP_REGISTER_MISC_OFF_BASE,
	STEP_REGISTER_CFG_ON_BASE,
	STEP_CIRQ_BASE,
	STEP_DA_XOBUF_BASE,
	STEP_REGISTER_MAX,
};

struct step_register_base_struct {
	unsigned long address;
	unsigned long long size;
};

struct step_action_list {
	struct list_head list;
};

struct step_pd_entry {
	bool is_enable;
	unsigned int expires_ms;
	struct step_action_list action_list;
	OSAL_TIMER dump_timer;
	struct delayed_work pd_work;
	struct list_head list;
};

#define STP_STEP_PD_RECORD_SIZE 32
struct step_pd_record {
	OSAL_UNSLEEPABLE_LOCK mutex;
	unsigned int write;
	unsigned int read;
	unsigned int size;
	struct step_pd_entry *queue[STP_STEP_PD_RECORD_SIZE];
};

struct step_pd_struct {
	bool is_init;
	struct workqueue_struct *step_pd_wq;
	struct list_head pd_list;
};

struct step_action {
	struct list_head list;
	enum step_action_id action_id;
};

typedef int (*STEP_WRITE_ACT_TO_LIST) (struct step_action_list *, enum step_action_id, char **);
typedef void (*STEP_DO_EXTRA) (unsigned int, ...);

struct step_emi_action {
	bool is_write;
	unsigned int begin_offset;
	unsigned int end_offset;
	int value;
	struct step_action base;
};

struct step_register_action {
	bool is_write;
	enum step_register_base_id address_type;
	unsigned long address;
	unsigned int offset;
	unsigned int times;
	unsigned int delay_time;
	int value;
	struct step_action base;
};

struct step_gpio_action {
	bool is_write;
	unsigned int pin_symbol;
	struct step_action base;
};

struct step_disable_reset_action {
	struct step_action base;
};

struct step_chip_reset_action {
	struct step_action base;
};

struct step_keep_wakeup_action {
	struct step_action base;
};

struct step_cancel_wakeup_action {
	struct step_action base;
};

struct step_periodic_dump_action {
	struct step_pd_entry *pd_entry;
	struct step_action base;
};

#define list_entry_emi_action(ptr) \
	container_of(ptr, struct step_emi_action, base)
#define list_entry_register_action(ptr) \
	container_of(ptr, struct step_register_action, base)
#define list_entry_gpio_action(ptr) \
	container_of(ptr, struct step_gpio_action, base)
#define list_entry_drst_action(ptr) \
	container_of(ptr, struct step_disable_reset_action, base)
#define list_entry_crst_action(ptr) \
	container_of(ptr, struct step_chip_reset_action, base)
#define list_entry_kwak_action(ptr) \
	container_of(ptr, struct step_keep_wakeup_action, base)
#define list_entry_cwak_action(ptr) \
	container_of(ptr, struct step_cancel_wakeup_action, base)
#define list_entry_pd_action(ptr) \
		container_of(ptr, struct step_periodic_dump_action, base)

struct step_reg_addr_info {
	int address_type;
	unsigned long address;
};

struct step_target_act_list_info {
	enum step_trigger_point_id tp_id;
	struct step_action_list *p_target_list;
	struct step_pd_entry *p_pd_entry;
};

#define STEP_PARAMETER_SIZE 10
struct step_parse_line_data_param_info {
	int state;
	enum step_action_id act_id;
	char *act_params[STEP_PARAMETER_SIZE];
	int param_index;
};

typedef struct step_action *(*STEP_CREATE_ACTION) (char *[]);
typedef int (*STEP_DO_ACTIONS) (struct step_action *, STEP_DO_EXTRA);
typedef void (*STEP_REMOVE_ACTION) (struct step_action *);
struct step_action_contrl {
	STEP_CREATE_ACTION func_create_action;
	STEP_DO_ACTIONS func_do_action;
	STEP_REMOVE_ACTION func_remove_action;
};

struct step_env_struct {
	bool is_enable;
	bool is_keep_wakeup;
	struct step_action_list actions[STEP_TRIGGER_POINT_MAX];
	unsigned char __iomem *emi_base_addr;
	struct step_register_base_struct reg_base[STEP_REGISTER_MAX];
	struct step_pd_struct pd_struct;
};

/********************************************************************************
 *              F U N C T I O N   D E C L A R A T I O N S
*********************************************************************************/
void wmt_step_init(void);
void wmt_step_deinit(void);
void wmt_step_do_actions(enum step_trigger_point_id tp_id);
void wmt_step_func_crtl_do_actions(ENUM_WMTDRV_TYPE_T type, ENUM_WMT_OPID_T opId);
void wmt_step_command_timeout_do_actions(char *reason);
#ifdef CFG_WMT_STEP
#define WMT_STEP_INIT_FUNC wmt_step_init
#define WMT_STEP_DEINIT_FUNC wmt_step_deinit
#define WMT_STEP_DO_ACTIONS_FUNC wmt_step_do_actions
#define WMT_STEP_FUNC_CTRL_DO_ACTIONS_FUNC wmt_step_func_crtl_do_actions
#define WMT_STEP_COMMAND_TIMEOUT_DO_ACTIONS_FUNC wmt_step_command_timeout_do_actions
#else
#define WMT_STEP_INIT_FUNC
#define WMT_STEP_DEINIT_FUNC
#define WMT_STEP_DO_ACTIONS_FUNC
#define WMT_STEP_FUNC_CTRL_DO_ACTIONS_FUNC
#define WMT_STEP_COMMAND_TIMEOUT_DO_ACTIONS_FUNC
#endif

/********************************************************************************
 *              D E C L A R E   F O R   T E S T
*********************************************************************************/
int wmt_step_read_file(const char *file_name);
int wmt_step_parse_data(const char *in_buf, unsigned int size, STEP_WRITE_ACT_TO_LIST func_act_to_list);
int wmt_step_init_pd_env(void);
int wmt_step_deinit_pd_env(void);
struct step_pd_entry *wmt_step_get_periodic_dump_entry(unsigned int expires);
struct step_action *wmt_step_create_action(enum step_action_id act_id, char *params[]);
int wmt_step_do_emi_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_register_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_gpio_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_disable_reset_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_chip_reset_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_keep_wakeup_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_cancel_wakeup_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
int wmt_step_do_periodic_dump_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra);
void wmt_step_remove_action(struct step_action *p_act);

#endif /* end of _WMT_STEP_H_ */

