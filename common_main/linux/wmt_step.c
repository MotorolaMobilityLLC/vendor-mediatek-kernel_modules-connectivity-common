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

#include <connectivity_build_in_adapter.h>
#include <linux/platform_device.h>

#include "osal.h"
#include "wmt_step.h"
#include "wmt_dev.h"
#include "wmt_plat.h"
#include "mtk_wcn_consys_hw.h"
#include "stp_core.h"
#include "wmt_lib.h"

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************/
static struct step_action *wmt_step_create_emi(char *params[]);
static struct step_action *wmt_step_create_register(char *params[]);
static struct step_action *wmt_step_create_gpio(char *params[]);
static struct step_action *wmt_step_create_disable_reset(char *params[]);
static struct step_action *wmt_step_create_chip_reset(char *params[]);
static struct step_action *wmt_step_create_keep_wakeup(char *params[]);
static struct step_action *wmt_step_create_cancel_wakeup(char *params[]);

static void wmt_step_remove_emi_action(struct step_action *p_act);
static void wmt_step_remove_register_action(struct step_action *p_act);
static void wmt_step_remove_gpio_action(struct step_action *p_act);
static void wmt_step_remove_disable_reset_action(struct step_action *p_act);
static void wmt_step_remove_chip_reset_action(struct step_action *p_act);
static void wmt_step_remove_keep_wakeup_action(struct step_action *p_act);
static void wmt_step_remove_cancel_wakeup_action(struct step_action *p_act);

static int wmt_step_access_line_state_init(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);
static int wmt_step_access_line_state_tp(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);
static int wmt_step_access_line_state_at(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);
static int wmt_step_access_line_state_at_op(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info);

/*******************************************************************************
 *                           D E F I N E
********************************************************************************/
#define STEP_EMI_ACT_INT (int)(*(int *)STEP_ACTION_NAME_EMI)
#define STEP_REG_ACT_INT (int)(*(int *)STEP_ACTION_NAME_REGISTER)
#define STEP_GPIO_ACT_INT (int)(*(int *)STEP_ACTION_NAME_GPIO)
#define STEP_DISABLE_RESET_ACT_INT (int)(*(int *)STEP_ACTION_NAME_DISABLE_RESET)
#define STEP_CHIP_RESET_ACT_INT (int)(*(int *)STEP_ACTION_NAME_CHIP_RESET)
#define STEP_KEEP_WAKEUP_ACT_INT (int)(*(int *)STEP_ACTION_NAME_KEEP_WAKEUP)
#define STEP_CANCEL_KEEP_WAKEUP_ACT_INT (int)(*(int *)STEP_ACTION_NAME_CANCEL_WAKEUP)

#define STEP_PARSE_LINE_STATE_INIT 0
#define STEP_PARSE_LINE_STATE_TP 1
#define STEP_PARSE_LINE_STATE_AT 2
#define STEP_PARSE_LINE_STATE_AT_OP 3
/*******************************************************************************
 *                           P R I V A T E   D A T A
********************************************************************************/
struct step_env_struct g_step_env;

static const struct step_action_contrl wmt_step_action_map[] = {
	[STEP_ACTION_INDEX_EMI] = {
		wmt_step_create_emi,
		wmt_step_do_emi_action,
		wmt_step_remove_emi_action
	},
	[STEP_ACTION_INDEX_REGISTER] = {
		wmt_step_create_register,
		wmt_step_do_register_action,
		wmt_step_remove_register_action
	},
	[STEP_ACTION_INDEX_GPIO] = {
		wmt_step_create_gpio,
		wmt_step_do_gpio_action,
		wmt_step_remove_gpio_action
	},
	[STEP_ACTION_INDEX_DISABLE_RESET] = {
		wmt_step_create_disable_reset,
		wmt_step_do_disable_reset_action,
		wmt_step_remove_disable_reset_action
	},
	[STEP_ACTION_INDEX_CHIP_RESET] = {
		wmt_step_create_chip_reset,
		wmt_step_do_chip_reset_action,
		wmt_step_remove_chip_reset_action
	},
	[STEP_ACTION_INDEX_KEEP_WAKEUP] = {
		wmt_step_create_keep_wakeup,
		wmt_step_do_keep_wakeup_action,
		wmt_step_remove_keep_wakeup_action
	},
	[STEP_ACTION_INDEX_CANCEL_WAKEUP] = {
		wmt_step_create_cancel_wakeup,
		wmt_step_do_cancel_wakeup_action,
		wmt_step_remove_cancel_wakeup_action
	},
};

static const char * const STEP_TRIGGER_TIME_NAME[] = {
	[STEP_TRIGGER_POINT_COMMAND_TIMEOUT] =
		"[TP 1] When Command timeout",
	[STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT] =
		"[TP 2] When Firmware trigger assert",
	[STEP_TRIGGER_POINT_BEFORE_CHIP_RESET] =
		"[TP 3] Before Chip reset",
	[STEP_TRIGGER_POINT_AFTER_CHIP_RESET] =
		"[TP 4] After Chip reset",
	[STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_ON] =
		"[TP 5] Before Wifi function on",
	[STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_OFF] =
		"[TP 6] Before Wifi function off",
	[STEP_TRIGGER_POINT_BEFORE_BT_FUNC_ON] =
		"[TP 7] Before BT function on",
	[STEP_TRIGGER_POINT_BEFORE_BT_FUNC_OFF] =
		"[TP 8] Before BT function off",
	[STEP_TRIGGER_POINT_BEFORE_FM_FUNC_ON] =
		"[TP 9] Before FM function on",
	[STEP_TRIGGER_POINT_BEFORE_FM_FUNC_OFF] =
		"[TP 10] Before FM function off",
	[STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_ON] =
		"[TP 11] Before GPS function on",
	[STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_OFF] =
		"[TP 12] Before GPS function off",
	[STEP_TRIGGER_POINT_BEFORE_READ_THERMAL] =
		"[TP 13] Before read consys thermal",
	[STEP_TRIGGER_POINT_POWER_ON_START] =
		"[TP 14] Power on sequence(0): Start power on",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_GET_CONNSYS_ID] =
		"[TP 15] Power on sequence(1): Before can get connsys id",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_SEND_DOWNLOAD_PATCH] =
		"[TP 16] Power on sequence(2): Before send download patch",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_CONNSYS_RESET] =
		"[TP 17] Power on sequence(3): Before connsys reset (donwload patch)",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_SET_WIFI_LTE_COEX] =
		"[TP 18] Power on sequence(4): Before set wifi and lte coex",
	[STEP_TRIGGER_POINT_POWER_ON_BEFORE_BT_WIFI_CALIBRATION] =
		"[TP 19] Power on sequence(5): Before set BT and Wifi calibration",
	[STEP_TRIGGER_POINT_POWER_ON_END] =
		"[TP 20] Power on sequence(6): End power on",
	[STEP_TRIGGER_POINT_BEFORE_POWER_OFF] =
		"[TP 21] Before WMT power off",
	[STEP_TRIGGER_POINT_WHEN_AP_SUSPEND] =
		"[TP 22] When AP suspend",
	[STEP_TRIGGER_POINT_WHEN_AP_RESUME] =
		"[TP 23] When AP resume",
};

static const int wmt_step_func_ctrl_id[WMTDRV_TYPE_MAX][2] = {
	[WMTDRV_TYPE_BT] = {
		STEP_TRIGGER_POINT_BEFORE_BT_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_BT_FUNC_ON
	},
	[WMTDRV_TYPE_FM] = {
		STEP_TRIGGER_POINT_BEFORE_FM_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_FM_FUNC_ON
	},
	[WMTDRV_TYPE_GPS] = {
		STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_GPS_FUNC_ON
	},
	[WMTDRV_TYPE_WIFI] = {
		STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_OFF,
		STEP_TRIGGER_POINT_BEFORE_WIFI_FUNC_ON
	},
};

typedef int (*STEP_LINE_STATE) (char *,
	struct step_target_act_list_info *, struct step_parse_line_data_param_info *);
static const STEP_LINE_STATE wmt_step_line_state_action_map[] = {
	[STEP_PARSE_LINE_STATE_INIT] = wmt_step_access_line_state_init,
	[STEP_PARSE_LINE_STATE_TP] = wmt_step_access_line_state_tp,
	[STEP_PARSE_LINE_STATE_AT] = wmt_step_access_line_state_at,
	[STEP_PARSE_LINE_STATE_AT_OP] = wmt_step_access_line_state_at_op,
};

/*******************************************************************************
 *                      I N T E R N A L   F U N C T I O N S
********************************************************************************/
static void wmt_step_init_list(void)
{
	unsigned int i = 0;

	for (i = 0; i < STEP_TRIGGER_POINT_MAX; i++)
		INIT_LIST_HEAD(&(g_step_env.actions[i].list));
}

static unsigned char __iomem *wmt_step_get_emi_base_address(void)
{
	if (g_step_env.emi_base_addr == NULL) {
		if (gConEmiPhyBase)
			g_step_env.emi_base_addr = ioremap_nocache(gConEmiPhyBase, gConEmiSize);
	}

	return g_step_env.emi_base_addr;
}

static void _wmt_step_init_register_base_size(struct device_node *node, int index, int step_index, unsigned long addr)
{
	int flag;

	of_get_address(node, index, &(g_step_env.reg_base[step_index].size), &flag);
	g_step_env.reg_base[step_index].address = addr;
}

static void wmt_step_init_register_base_size(void)
{
	struct device_node *node = NULL;

	if (g_pdev != NULL) {
		node = g_pdev->dev.of_node;
		_wmt_step_init_register_base_size(node, MCU_BASE_INDEX,
			STEP_REGISTER_CONN_MCU_CONFIG_BASE, conn_reg.mcu_base);
		_wmt_step_init_register_base_size(node, TOP_RGU_BASE_INDEX,
			STEP_REGISTER_AP_RGU_BASE, conn_reg.ap_rgu_base);
		_wmt_step_init_register_base_size(node, INFRACFG_AO_BASE_INDEX,
			STEP_REGISTER_TOPCKGEN_BASE, conn_reg.topckgen_base);
		_wmt_step_init_register_base_size(node, SPM_BASE_INDEX,
			STEP_REGISTER_SPM_BASE, conn_reg.spm_base);
		_wmt_step_init_register_base_size(node, MCU_CONN_HIF_ON_BASE_INDEX,
			STEP_REGISTER_HIF_ON_BASE, conn_reg.mcu_conn_hif_on_base);
		_wmt_step_init_register_base_size(node, MCU_TOP_MISC_OFF_BASE_INDEX,
			STEP_REGISTER_MISC_OFF_BASE, conn_reg.mcu_top_misc_off_base);
		_wmt_step_init_register_base_size(node, MCU_CFG_ON_BASE_INDEX,
			STEP_REGISTER_CFG_ON_BASE, conn_reg.mcu_cfg_on_base);
		_wmt_step_init_register_base_size(node, MCU_CIRQ_BASE_INDEX,
			STEP_CIRQ_BASE, conn_reg.mcu_cirq_base);
	}
}

static void wmt_step_clear_action_list(struct step_action_list *action_list)
{
	struct step_action *p_act, *p_act_next;

	list_for_each_entry_safe(p_act, p_act_next, &(action_list->list), list) {
		list_del_init(&p_act->list);
		wmt_step_remove_action(p_act);
	}
}

static void wmt_step_clear_list(void)
{
	unsigned int i = 0;

	for (i = 0; i < STEP_TRIGGER_POINT_MAX; i++)
		wmt_step_clear_action_list(&g_step_env.actions[i]);
}

static void wmt_step_unioremap_emi(void)
{
	if (g_step_env.emi_base_addr != NULL) {
		iounmap(g_step_env.emi_base_addr);
		g_step_env.emi_base_addr = NULL;
	}
}

static unsigned char *mtk_step_get_emi_virt_addr(unsigned char *emi_base_addr, unsigned int offset)
{
	unsigned char *p_virtual_addr = NULL;

	if (offset > gConEmiSize) {
		WMT_ERR_FUNC("STEP failed: offset size %d over MAX size(%d)\n", offset,
			gConEmiSize);
		return NULL;
	}
	p_virtual_addr = emi_base_addr + offset;

	return p_virtual_addr;
}

static int wmt_step_get_cfg(const char *p_patch_name, osal_firmware **pp_patch)
{
	osal_firmware *fw = NULL;

	*pp_patch = NULL;
	if (request_firmware((const struct firmware **)&fw, p_patch_name, NULL) != 0)
		return -1;

	WMT_DBG_FUNC("Load step cfg %s ok!!\n", p_patch_name);
	*pp_patch = fw;

	return 0;
}

static unsigned char wmt_step_to_upper(char str)
{
	if ((str >= 'a') && (str <= 'z'))
		return str + ('A' - 'a');
	else
		return str;
}

static void wmt_step_string_to_upper(char *tok)
{
	for (; *tok != '\0'; tok++)
		*tok = wmt_step_to_upper(*tok);
}

static int wmt_step_get_int_from_four_char(char *str)
{
	unsigned char char_array[4];
	int i;

	for (i = 0; i < 4; i++) {
		if (*(str + i) == '\0')
			return -1;

		char_array[i] = wmt_step_to_upper(*(str + i));
	}

	return *(int *)char_array;
}

static enum step_trigger_point_id wmt_step_parse_tp_id(char *str)
{
	long tp_id = STEP_TRIGGER_POINT_NO_DEFINE;

	if (osal_strtol(str, 10, &tp_id)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n", str);
		return STEP_TRIGGER_POINT_NO_DEFINE;
	}
	if (tp_id <= STEP_TRIGGER_POINT_NO_DEFINE || tp_id >= STEP_TRIGGER_POINT_MAX)
		return STEP_TRIGGER_POINT_NO_DEFINE;

	return (enum step_trigger_point_id)tp_id;
}

static int wmt_step_parse_act_id(char *str)
{
	int str_to_int = STEP_ACTION_INDEX_NO_DEFINE;

	if (str == NULL || str == '\0')
		return STEP_ACTION_INDEX_NO_DEFINE;

	str_to_int = wmt_step_get_int_from_four_char(str);
	if (str_to_int == STEP_EMI_ACT_INT)
		return STEP_ACTION_INDEX_EMI;
	else if (str_to_int == STEP_REG_ACT_INT)
		return STEP_ACTION_INDEX_REGISTER;
	else if (str_to_int == STEP_GPIO_ACT_INT)
		return STEP_ACTION_INDEX_GPIO;
	else if (str_to_int == STEP_DISABLE_RESET_ACT_INT)
		return STEP_ACTION_INDEX_DISABLE_RESET;
	else if (str_to_int == STEP_CHIP_RESET_ACT_INT)
		return STEP_ACTION_INDEX_CHIP_RESET;
	else if (str_to_int == STEP_KEEP_WAKEUP_ACT_INT)
		return STEP_ACTION_INDEX_KEEP_WAKEUP;
	else if (str_to_int == STEP_CANCEL_KEEP_WAKEUP_ACT_INT)
		return STEP_ACTION_INDEX_CANCEL_WAKEUP;
	else
		return STEP_ACTION_INDEX_NO_DEFINE;

}

static struct step_action_list *wmt_step_get_tp_list(int tp_id)
{
	if (tp_id <= STEP_TRIGGER_POINT_NO_DEFINE || tp_id >= STEP_TRIGGER_POINT_MAX) {
		WMT_ERR_FUNC("STEP failed: Write action to tp_id: %d\n", tp_id);
		return NULL;
	}

	return &g_step_env.actions[tp_id];
}

#define STEP_PARSE_LINE_RET_CONTINUE 0
#define STEP_PARSE_LINE_RET_BREAK 1

static void wmt_step_set_line_state(int *p_state, int value)
{
	*p_state = value;
}

static int wmt_step_access_line_state_init(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	wmt_step_string_to_upper(tok);
	if (osal_strcmp(tok, "[TP") == 0) {
		wmt_step_set_line_state(&p_parse_line_info->state, STEP_PARSE_LINE_STATE_TP);
		return STEP_PARSE_LINE_RET_CONTINUE;
	}

	if (p_parse_info->tp_id == STEP_TRIGGER_POINT_NO_DEFINE) {
		WMT_ERR_FUNC("STEP failed: Set trigger point first: %s\n", tok);
		return STEP_PARSE_LINE_RET_BREAK;
	}

	if (osal_strcmp(tok, "[AT]") == 0) {
		wmt_step_set_line_state(&p_parse_line_info->state, STEP_PARSE_LINE_STATE_AT);
		return STEP_PARSE_LINE_RET_CONTINUE;
	}

	return STEP_PARSE_LINE_RET_BREAK;
}

static int wmt_step_access_line_state_tp(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	char *pch;

	pch = osal_strchr(tok, ']');
	if (pch == NULL) {
		WMT_ERR_FUNC("STEP failed: Trigger point format is wrong: %s\n", tok);
	} else {
		*pch = '\0';
		p_parse_info->tp_id = wmt_step_parse_tp_id(tok);
		p_parse_info->p_target_list = wmt_step_get_tp_list(p_parse_info->tp_id);

		if (p_parse_info->tp_id == STEP_TRIGGER_POINT_NO_DEFINE)
			WMT_ERR_FUNC("STEP failed: Trigger point no define: %s\n", tok);
	}

	return STEP_PARSE_LINE_RET_BREAK;
}

static int wmt_step_access_line_state_at(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	p_parse_line_info->act_id = wmt_step_parse_act_id(tok);
	if (p_parse_line_info->act_id == STEP_ACTION_INDEX_NO_DEFINE) {
		WMT_ERR_FUNC("STEP failed: Action no define: %s\n", tok);
		return STEP_PARSE_LINE_RET_BREAK;
	}
	wmt_step_set_line_state(&p_parse_line_info->state, STEP_PARSE_LINE_STATE_AT_OP);

	return STEP_PARSE_LINE_RET_CONTINUE;
}

static int wmt_step_access_line_state_at_op(char *tok,
	struct step_target_act_list_info *p_parse_info,
	struct step_parse_line_data_param_info *p_parse_line_info)
{
	p_parse_line_info->act_params[p_parse_line_info->param_index] = tok;
	(p_parse_line_info->param_index)++;

	return STEP_PARSE_LINE_RET_CONTINUE;
}

static void wmt_step_parse_line_data(char *line, struct step_target_act_list_info *p_parse_info,
	STEP_WRITE_ACT_TO_LIST func_act_to_list)
{
	char *tok;
	int line_ret = STEP_PARSE_LINE_RET_BREAK;
	struct step_parse_line_data_param_info parse_line_info;

	parse_line_info.param_index = 0;
	parse_line_info.act_id = STEP_ACTION_INDEX_NO_DEFINE;
	parse_line_info.state = STEP_PARSE_LINE_STATE_INIT;

	while ((tok = osal_strsep(&line, " \t")) != NULL) {
		if (*tok == '\0')
			continue;
		if (osal_strcmp(tok, "//") == 0)
			break;

		if (wmt_step_line_state_action_map[parse_line_info.state] != NULL) {
			line_ret = wmt_step_line_state_action_map[parse_line_info.state] (tok,
				p_parse_info, &parse_line_info);
		}

		if (line_ret == STEP_PARSE_LINE_RET_CONTINUE)
			continue;
		else
			break;
	}

	if (parse_line_info.state == STEP_PARSE_LINE_STATE_AT_OP) {
		func_act_to_list(p_parse_info->p_target_list,
			parse_line_info.act_id, parse_line_info.act_params);
	}
}

static void _wmt_step_do_actions(struct step_action_list *action_list)
{
	struct step_action *p_act, *p_act_next;

	list_for_each_entry_safe(p_act, p_act_next, &action_list->list, list) {
		if (p_act->action_id <= STEP_ACTION_INDEX_NO_DEFINE || p_act->action_id >= STEP_ACTION_INDEX_MAX) {
			WMT_ERR_FUNC("STEP failed: Wrong action id %d\n", (int)p_act->action_id);
			continue;
		}

		if (wmt_step_action_map[p_act->action_id].func_do_action != NULL)
			wmt_step_action_map[p_act->action_id].func_do_action(p_act, NULL);
		else
			WMT_ERR_FUNC("STEP failed: Action is NULL\n");
	}
}

static void wmt_step_print_trigger_time(enum step_trigger_point_id tp_id, char *reason)
{
	const char *p_trigger_name = NULL;

	p_trigger_name = STEP_TRIGGER_TIME_NAME[tp_id];
	if (reason != NULL)
		WMT_INFO_FUNC("STEP show: Trigger point: %s reason: %s\n", p_trigger_name, reason);
	else
		WMT_INFO_FUNC("STEP show: Trigger point: %s\n", p_trigger_name);
}

static VOID wmt_step_do_actions_from_tp(enum step_trigger_point_id tp_id, char *reason)
{
	if (g_step_env.is_enable == 0)
		return;

	if (tp_id <= STEP_TRIGGER_POINT_NO_DEFINE || tp_id >= STEP_TRIGGER_POINT_MAX) {
		WMT_ERR_FUNC("STEP failed: Do actions from tp_id: %d\n", tp_id);
		return;
	}

	if (list_empty(&g_step_env.actions[tp_id].list))
		return;

	wmt_step_print_trigger_time(tp_id, reason);
	_wmt_step_do_actions(&g_step_env.actions[tp_id]);
}

static int wmt_step_write_action(struct step_action_list *p_list, enum step_action_id act_id,
	char *params[])
{
	struct step_action *p_action;

	if (p_list == NULL) {
		WMT_ERR_FUNC("STEP failed: p_list is null\n");
		return -1;
	}

	p_action = wmt_step_create_action(act_id, params);
	if (p_action != NULL) {
		list_add_tail(&(p_action->list), &(p_list->list));
		return 0;
	}

	return -1;
}

static int wmt_step_parse_register_address(struct step_reg_addr_info *p_reg_addr, char *ptr, long offset)
{
	unsigned long res;
	unsigned int symbol;

	if (*ptr == '#') {
		if (osal_strtol(ptr + 1, 10, &res)) {
			WMT_ERR_FUNC("STEP failed: str to value %s\n", ptr);
			return -1;
		}
		symbol = (unsigned int) res;
		if (symbol <= STEP_REGISTER_PHYSICAL_ADDRESS || symbol >= STEP_REGISTER_MAX) {
			WMT_ERR_FUNC("STEP failed: No support the base %s\n", ptr);
			return -1;
		}
		res = g_step_env.reg_base[symbol].address;

		if (res == 0) {
			WMT_ERR_FUNC("STEP failed: No support the base %s\n", ptr);
			return -1;
		}

		if (offset >= g_step_env.reg_base[symbol].size) {
			WMT_ERR_FUNC("STEP failed: No symbol(%d), offset(%l) over max size(%llu) %s\n",
				symbol, offset,	g_step_env.reg_base[symbol].size);
			return -1;
		}

		p_reg_addr->address = res;
		p_reg_addr->address_type = symbol;
	} else {
		if (osal_strtol(ptr, 0, &res)) {
			WMT_ERR_FUNC("STEP failed: str to value %s\n", ptr);
			return -1;
		}
		p_reg_addr->address = res;
		p_reg_addr->address_type = STEP_REGISTER_PHYSICAL_ADDRESS;
	}

	return 0;
}

static struct step_action *wmt_step_create_emi(char *params[])
{
	struct step_emi_action *p_emi_act = NULL;
	long write, begin, end;

	if (params[0] == NULL || params[1] == NULL || params[2] == NULL) {
		WMT_ERR_FUNC("STEP failed: Init EMI param: %s, %s, %s\n",
			params[0], params[1], params[2]);
		return NULL;
	}

	if (osal_strtol(params[0], 0, &write) ||
		osal_strtol(params[1], 0, &begin) ||
		osal_strtol(params[2], 0, &end)) {
		WMT_ERR_FUNC("STEP failed: str to value %s, %s, %s\n",
			params[0], params[1], params[2]);
		return NULL;
	}

	p_emi_act = kzalloc(sizeof(struct step_emi_action), GFP_KERNEL);
	if (p_emi_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc emi fail\n");
		return NULL;
	}

	p_emi_act->is_write = write;
	p_emi_act->begin_offset = begin;
	p_emi_act->end_offset = end;

	return &(p_emi_act->base);
}

static struct step_action *wmt_step_create_read_register(int write, char *params[])
{
	struct step_register_action *p_reg_act = NULL;
	struct step_reg_addr_info reg_addr_info;
	long offset, times, delay_time;

	if (params[1] == NULL || params[2] == NULL ||
		params[3] == NULL || params[4] == NULL) {
		WMT_ERR_FUNC("STEP failed: Init read register param: %s, %s, %s, %s, %s\n",
			params[0], params[1], params[2], params[3], params[4]);
		return NULL;
	}

	if (osal_strtol(params[2], 0, &offset) ||
		osal_strtol(params[3], 0, &times) ||
		osal_strtol(params[4], 0, &delay_time)) {
		WMT_ERR_FUNC("STEP failed: str to value %s, %s, %s, %s, %s\n",
			params[0], params[1], params[2], params[3], params[4]);
		return NULL;
	}

	if (wmt_step_parse_register_address(&reg_addr_info, params[1], offset) == -1) {
		WMT_ERR_FUNC("STEP failed: Init read register symbol: %s\n", params[1]);
		return NULL;
	}

	p_reg_act = kzalloc(sizeof(struct step_register_action), GFP_KERNEL);
	if (p_reg_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc register fail\n");
		return NULL;
	}
	p_reg_act->is_write = write;
	p_reg_act->address_type = reg_addr_info.address_type;
	p_reg_act->address = reg_addr_info.address;
	p_reg_act->offset = offset;
	p_reg_act->times = times;
	p_reg_act->delay_time = delay_time;

	return &(p_reg_act->base);
}

static struct step_action *wmt_step_create_write_register(int write, char *params[])
{
	struct step_register_action *p_reg_act = NULL;
	struct step_reg_addr_info reg_addr_info;
	long offset, value;

	if (params[1] == NULL || params[2] == NULL || params[3] == NULL) {
		WMT_ERR_FUNC("STEP failed: Init write register param: %s, %s, %s, %s\n",
			params[0], params[1], params[2], params[3]);
		return NULL;
	}

	if (osal_strtol(params[2], 0, &offset) ||
		osal_strtol(params[3], 0, &value)) {
		WMT_ERR_FUNC("STEP failed: str to value %s, %s, %s, %s\n",
			params[0], params[1], params[2], params[3]);
		return NULL;
	}

	if (wmt_step_parse_register_address(&reg_addr_info, params[1], offset) == -1) {
		WMT_ERR_FUNC("STEP failed: init write register symbol: %s\n", params[1]);
		return NULL;
	}

	p_reg_act = kzalloc(sizeof(struct step_register_action), GFP_KERNEL);
	if (p_reg_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc register fail\n");
		return NULL;
	}
	p_reg_act->is_write = write;
	p_reg_act->address_type = reg_addr_info.address_type;
	p_reg_act->address = reg_addr_info.address;
	p_reg_act->offset = offset;
	p_reg_act->value = value;

	return &(p_reg_act->base);
}

static struct step_action *wmt_step_create_register(char *params[])
{
	struct step_register_action *p_reg_act = NULL;
	long write;

	if (params[0] == NULL) {
		WMT_ERR_FUNC("STEP failed: Register no params\n");
		return NULL;
	}

	if (osal_strtol(params[0], 0, &write)) {
		WMT_ERR_FUNC("STEP failed: str to value %s\n",
			params[0]);
		return NULL;
	}

	if (write == 0)
		return wmt_step_create_read_register(write, params);
	else
		return wmt_step_create_write_register(write, params);

	return &(p_reg_act->base);
}

static struct step_action *wmt_step_create_gpio(char *params[])
{
	struct step_gpio_action *p_gpio_act = NULL;
	long write, symbol;

	if (params[0] == NULL || params[1] == NULL) {
		WMT_ERR_FUNC("STEP failed: init gpio param: %s %s\n", params[0], params[1]);
		return NULL;
	}

	if (osal_strtol(params[0], 0, &write) ||
		osal_strtol(params[1], 0, &symbol)) {
		WMT_ERR_FUNC("STEP failed: str to value %s, %s\n",
			params[0], params[1]);
		return NULL;
	}

	p_gpio_act = kzalloc(sizeof(struct step_gpio_action), GFP_KERNEL);
	if (p_gpio_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc gpio fail\n");
		return NULL;
	}
	p_gpio_act->is_write = write;
	p_gpio_act->pin_symbol = symbol;

	return &(p_gpio_act->base);
}

static struct step_action *wmt_step_create_disable_reset(char *params[])
{
	struct step_disable_reset_action *p_drst_act = NULL;

	p_drst_act = kzalloc(sizeof(struct step_disable_reset_action), GFP_KERNEL);
	if (p_drst_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc disalbe reset fail\n");
		return NULL;
	}

	return &(p_drst_act->base);
}

static struct step_action *wmt_step_create_chip_reset(char *params[])
{
	struct step_chip_reset_action *p_crst_act = NULL;

	p_crst_act = kzalloc(sizeof(struct step_chip_reset_action), GFP_KERNEL);
	if (p_crst_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc chip reset fail\n");
		return NULL;
	}

	return &(p_crst_act->base);
}

static struct step_action *wmt_step_create_keep_wakeup(char *params[])
{
	struct step_keep_wakeup_action *p_kwak_act = NULL;

	p_kwak_act = kzalloc(sizeof(struct step_keep_wakeup_action), GFP_KERNEL);
	if (p_kwak_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc keep wakeup fail\n");
		return NULL;
	}

	return &(p_kwak_act->base);
}

static struct step_action *wmt_step_create_cancel_wakeup(char *params[])
{
	struct step_cancel_wakeup_action *p_cwak_act = NULL;

	p_cwak_act = kzalloc(sizeof(struct step_cancel_wakeup_action), GFP_KERNEL);
	if (p_cwak_act == NULL) {
		WMT_ERR_FUNC("STEP failed: kzalloc cancel wakeup fail\n");
		return NULL;
	}

	return &(p_cwak_act->base);
}

static int wmt_step_do_write_register_action(struct step_register_action *p_reg_act,
	STEP_DO_EXTRA func_do_extra)
{
	phys_addr_t phy_addr;
	void __iomem *p_addr = NULL;
	SIZE_T vir_addr;

	if (p_reg_act->address_type == STEP_REGISTER_PHYSICAL_ADDRESS) {
		phy_addr = p_reg_act->address + p_reg_act->offset;
		if (phy_addr & 0x3) {
			WMT_ERR_FUNC("STEP failed: phy_addr(0x%08x) page failed\n", phy_addr);
			return -1;
		}

		p_addr = ioremap_nocache(phy_addr, 0x4);
		if (p_addr) {
			writel(p_reg_act->value, p_addr);
			WMT_INFO_FUNC(
				"STEP show: reg write Phy addr(0x%08x): 0x%08x\n",
				phy_addr, CONSYS_REG_READ(p_addr));
			if (func_do_extra != NULL)
				func_do_extra(1, CONSYS_REG_READ(p_addr));
			iounmap(p_addr);
		} else {
			WMT_ERR_FUNC("STEP failed: ioremap(0x%08x) is NULL\n", phy_addr);
			return -1;
		}
	} else {
		vir_addr = p_reg_act->address + p_reg_act->offset;
		if (vir_addr & 0x3) {
			WMT_ERR_FUNC("STEP failed: vir_addr(0x%08x) page failed\n", vir_addr);
			return -1;
		}

		CONSYS_REG_WRITE(vir_addr, p_reg_act->value);
		WMT_INFO_FUNC(
			"STEP show: reg write (symbol offset)(%d, 0x%08x): 0x%08x\n",
			p_reg_act->address_type, p_reg_act->offset,
			CONSYS_REG_READ(vir_addr));
		if (func_do_extra != NULL)
			func_do_extra(1, CONSYS_REG_READ(vir_addr));
	}

	return 0;
}

static void _wmt_step_do_read_register_action(struct step_register_action *p_reg_act,
	STEP_DO_EXTRA func_do_extra, char *info, int value)
{
	int i;

	for (i = 0; i < p_reg_act->times; i++) {
		if (i > 0)
			osal_sleep_ms(p_reg_act->delay_time);

		WMT_INFO_FUNC("%s", info);
		if (func_do_extra != NULL)
			func_do_extra(1, value);
	}
}

static int wmt_step_do_read_register_action(struct step_register_action *p_reg_act,
	STEP_DO_EXTRA func_do_extra)
{
	phys_addr_t phy_addr;
	void __iomem *p_addr = NULL;
	SIZE_T vir_addr;
	char buf[64];

	if (p_reg_act->address_type == STEP_REGISTER_PHYSICAL_ADDRESS) {
		phy_addr = p_reg_act->address + p_reg_act->offset;
		if (phy_addr & 0x3) {
			WMT_ERR_FUNC("STEP failed: phy_addr(0x%08x) page failed\n", phy_addr);
			return -1;
		}

		p_addr = ioremap_nocache(phy_addr, 0x4);
		if (p_addr) {
			sprintf(buf, "STEP show: reg read Phy addr(0x%08x): 0x%08x\n",
				(int)phy_addr, CONSYS_REG_READ(p_addr));
			_wmt_step_do_read_register_action(p_reg_act, func_do_extra, buf,
				CONSYS_REG_READ(p_addr));
			iounmap(p_addr);
		} else {
			WMT_ERR_FUNC("STEP failed: ioremap(0x%08x) is NULL\n", phy_addr);
			return -1;
		}
	} else {
		vir_addr = p_reg_act->address + p_reg_act->offset;
		if (vir_addr & 0x3) {
			WMT_ERR_FUNC("STEP failed: vir_addr(0x%08x) page failed\n", vir_addr);
			return -1;
		}

		sprintf(buf, "STEP show: reg read (symbol offset)(%d, 0x%08x): 0x%08x\n",
			p_reg_act->address_type, p_reg_act->offset,
			CONSYS_REG_READ(vir_addr));
		_wmt_step_do_read_register_action(p_reg_act, func_do_extra, buf,
			CONSYS_REG_READ(vir_addr));
	}

	return 0;
}

static bool wmt_step_reg_readable(struct step_register_action *p_reg_act)
{
	phys_addr_t phy_addr;

	/* Protect 0x18000000 ~ 0x18006FFF and 0x18008000 ~ 0x180FFFFF */
	if (p_reg_act->address_type == STEP_REGISTER_PHYSICAL_ADDRESS) {
		phy_addr = p_reg_act->address + p_reg_act->offset;
		if (phy_addr > 0x18000000 && phy_addr < 0x180FFFFF) {
			if (phy_addr >= 0x18007000 && phy_addr <= 0x18007FFF)
				return 1;

			return mtk_consys_check_reg_readable();
		}
	} else {
		if (p_reg_act->address_type == STEP_REGISTER_CONN_MCU_CONFIG_BASE ||
			p_reg_act->address_type == STEP_REGISTER_MISC_OFF_BASE ||
			p_reg_act->address_type == STEP_REGISTER_CFG_ON_BASE ||
			p_reg_act->address_type == STEP_CIRQ_BASE) {
			return mtk_consys_check_reg_readable();
		}
	}

	return 1;
}

static void wmt_step_remove_emi_action(struct step_action *p_act)
{
	struct step_emi_action *p_emi_act = NULL;

	p_emi_act = (struct step_emi_action *) list_entry_emi_action(p_act);
	kfree(p_emi_act);
}

static void wmt_step_remove_register_action(struct step_action *p_act)
{
	struct step_register_action *p_reg_act = NULL;

	p_reg_act =
		(struct step_register_action *) list_entry_register_action(p_act);
	kfree(p_reg_act);
}

static void wmt_step_remove_gpio_action(struct step_action *p_act)
{
	struct step_gpio_action *p_gpio_act = NULL;

	p_gpio_act = (struct step_gpio_action *) list_entry_gpio_action(p_act);
	kfree(p_gpio_act);
}

static void wmt_step_remove_disable_reset_action(struct step_action *p_act)
{
	struct step_disable_reset_action *p_drst = NULL;

	p_drst =
		(struct step_disable_reset_action *) list_entry_drst_action(p_act);
	kfree(p_drst);
}

static void wmt_step_remove_chip_reset_action(struct step_action *p_act)
{
	struct step_chip_reset_action *p_crst = NULL;

	p_crst = (struct step_chip_reset_action *) list_entry_crst_action(p_act);
	kfree(p_crst);
}

static void wmt_step_remove_keep_wakeup_action(struct step_action *p_act)
{
	struct step_keep_wakeup_action *p_kwak = NULL;

	p_kwak = (struct step_keep_wakeup_action *) list_entry_kwak_action(p_act);
	kfree(p_kwak);
}

static void wmt_step_remove_cancel_wakeup_action(struct step_action *p_act)
{
	struct step_cancel_wakeup_action *p_cwak = NULL;

	p_cwak =
		(struct step_cancel_wakeup_action *) list_entry_cwak_action(p_act);
	kfree(p_cwak);
}

/*******************************************************************************
 *              I N T E R N A L   F U N C T I O N S   W I T H   U T
********************************************************************************/
int wmt_step_do_emi_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_emi_action *p_emi_act = NULL;
	unsigned char *p_emi_begin_addr = NULL, *p_emi_end_addr = NULL;
	unsigned char __iomem *emi_base_addr = NULL;
	unsigned int dis = 0, temp = 0, i = 0;

	p_emi_act = (struct step_emi_action *) list_entry_emi_action(p_act);
	if (p_emi_act->is_write != 0) {
		WMT_ERR_FUNC("STEP failed: Only support dump EMI region\n");
		return -1;
	}

	if (p_emi_act->begin_offset > p_emi_act->end_offset) {
		temp = p_emi_act->begin_offset;
		p_emi_act->begin_offset = p_emi_act->end_offset;
		p_emi_act->end_offset = temp;
	}
	dis = p_emi_act->end_offset - p_emi_act->begin_offset;

	emi_base_addr = wmt_step_get_emi_base_address();
	if (emi_base_addr == NULL) {
		WMT_ERR_FUNC("STEP failed: EMI base address is NULL\n");
		return -1;
	}

	if (p_emi_act->begin_offset & 0x3) {
		WMT_ERR_FUNC("STEP failed: begin offset(0x%08x) page failed\n",
			p_emi_act->begin_offset);
		return -1;
	}

	p_emi_begin_addr = mtk_step_get_emi_virt_addr(emi_base_addr, p_emi_act->begin_offset);
	p_emi_end_addr = mtk_step_get_emi_virt_addr(emi_base_addr, p_emi_act->end_offset);
	if (!p_emi_begin_addr) {
		WMT_ERR_FUNC("STEP failed: Get NULL begin virtual address 0x%08x\n",
			p_emi_act->begin_offset);
		return -1;
	}

	if (!p_emi_end_addr) {
		WMT_ERR_FUNC("STEP failed: Get NULL end virtual address 0x%08x\n",
			p_emi_act->end_offset);
		return -1;
	}

	for (i = 0; i < dis; i += 0x4) {
		WMT_INFO_FUNC("STEP show: EMI action, Phy address(0x%08x): 0x%08x\n",
			(gConEmiPhyBase + p_emi_act->begin_offset + i),
			CONSYS_REG_READ(p_emi_begin_addr + i));
		if (func_do_extra != NULL)
			func_do_extra(1, CONSYS_REG_READ(p_emi_begin_addr + i));
	}

	return 0;
}

int wmt_step_do_register_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_register_action *p_reg_act = NULL;
	int ret = 0;
	bool is_wakeup = g_step_env.is_keep_wakeup;

	p_reg_act = (struct step_register_action *) list_entry_register_action(p_act);

	if (is_wakeup == 1) {
		if (DISABLE_PSM_MONITOR())
			WMT_ERR_FUNC("STEP failed: Wake up, continue to show register\n");
	}

	/* TODO: Remove this after support action condition */
	if (!wmt_step_reg_readable(p_reg_act)) {
		WMT_ERR_FUNC("STEP failed: register cant read\n");
		if (is_wakeup == 1)
			ENABLE_PSM_MONITOR();
		return -1;
	}

	if (p_reg_act->is_write == 1)
		ret = wmt_step_do_write_register_action(p_reg_act, func_do_extra);
	else
		ret = wmt_step_do_read_register_action(p_reg_act, func_do_extra);

	if (is_wakeup == 1)
		ENABLE_PSM_MONITOR();

	return ret;
}

int wmt_step_do_gpio_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	struct step_gpio_action *p_gpio_act = NULL;

	p_gpio_act = (struct step_gpio_action *) list_entry_gpio_action(p_act);
	if (p_gpio_act->is_write == 1) {
		WMT_ERR_FUNC("STEP failed: Only support dump GPIO\n");
		return -1;
	}

#ifdef KERNEL_gpio_dump_regs_range
	KERNEL_gpio_dump_regs_range(p_gpio_act->pin_symbol, p_gpio_act->pin_symbol);
#else
	WMT_INFO_FUNC("STEP show: No support gpio dump\n");
#endif
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_disable_reset_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	WMT_INFO_FUNC("STEP show: Do disable reset\n");
	mtk_wcn_stp_set_auto_rst(0);
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_chip_reset_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	WMT_INFO_FUNC("STEP show: Do chip reset\n");
	mtk_wcn_wmt_do_reset(WMTDRV_TYPE_WMT);
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_keep_wakeup_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	WMT_INFO_FUNC("STEP show: Do keep wake up\n");
	g_step_env.is_keep_wakeup = 1;
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

int wmt_step_do_cancel_wakeup_action(struct step_action *p_act, STEP_DO_EXTRA func_do_extra)
{
	WMT_INFO_FUNC("STEP show: Do cancel keep wake up\n");
	g_step_env.is_keep_wakeup = 0;
	if (func_do_extra != NULL)
		func_do_extra(0);

	return 0;
}

struct step_action *wmt_step_create_action(enum step_action_id act_id, char *params[])
{
	struct step_action *p_act = NULL;

	if (act_id <= STEP_ACTION_INDEX_NO_DEFINE || act_id >= STEP_ACTION_INDEX_MAX) {
		WMT_ERR_FUNC("STEP failed: Create action id: %d\n", act_id);
		return NULL;
	}

	if (wmt_step_action_map[act_id].func_create_action != NULL)
		p_act = wmt_step_action_map[act_id].func_create_action(params);
	else
		WMT_ERR_FUNC("STEP failed: Create no define id: %d\n", act_id);

	if (p_act != NULL)
		p_act->action_id = act_id;

	return p_act;
}

int wmt_step_parse_data(const char *in_buf, unsigned int size,
	STEP_WRITE_ACT_TO_LIST func_act_to_list)
{
	struct step_target_act_list_info parse_info;
	char *buf;
	char *line;

	buf = osal_malloc(size + 1);
	if (!buf) {
		WMT_ERR_FUNC("STEP failed: Buf malloc\n");
		return -1;
	}

	osal_memcpy(buf, (char *)in_buf, size);
	buf[size] = '\0';

	parse_info.tp_id = STEP_TRIGGER_POINT_NO_DEFINE;

	while ((line = osal_strsep(&buf, "\r\n")) != NULL)
		wmt_step_parse_line_data(line, &parse_info, func_act_to_list);

	osal_free(buf);

	return 0;
}


int wmt_step_read_file(const char *file_name)
{
	int ret = -1;
	const osal_firmware *p_step_cfg = NULL;

	if (g_step_env.is_enable == 1)
		return 0;

	if (0 == wmt_step_get_cfg(file_name, (osal_firmware **) &p_step_cfg)) {
		if (0 == wmt_step_parse_data((const char *)p_step_cfg->data, p_step_cfg->size,
			wmt_step_write_action)) {
			g_step_env.is_enable = 1;
			ret = 0;
		} else {
			g_step_env.is_enable = 0;
			ret = -1;
		}

		wmt_dev_patch_put((osal_firmware **) &p_step_cfg);
		return ret;
	}

	WMT_INFO_FUNC("STEP read file, %s is not exist\n", file_name);

	return ret;
}

void wmt_step_remove_action(struct step_action *p_act)
{
	if (p_act != NULL) {
		if (p_act->action_id <= STEP_ACTION_INDEX_NO_DEFINE || p_act->action_id >= STEP_ACTION_INDEX_MAX) {
			WMT_ERR_FUNC("STEP failed: Wrong action id %d\n", (int)p_act->action_id);
			return;
		}

		if (wmt_step_action_map[p_act->action_id].func_remove_action != NULL)
			wmt_step_action_map[p_act->action_id].func_remove_action(p_act);
	} else {
		WMT_ERR_FUNC("STEP failed: Action is NULL\n");
	}
}

/*******************************************************************************
 *                      E X T E R N A L   F U N C T I O N S
********************************************************************************/
void wmt_step_init(void)
{
	wmt_step_init_list();
	wmt_step_init_register_base_size();
	wmt_step_read_file(STEP_CONFIG_NAME);
}

void wmt_step_deinit(void)
{
	wmt_step_clear_list();
	wmt_step_unioremap_emi();
}

void wmt_step_do_actions(enum step_trigger_point_id tp_id)
{
	wmt_step_do_actions_from_tp(tp_id, NULL);
}

void wmt_step_func_crtl_do_actions(ENUM_WMTDRV_TYPE_T type, ENUM_WMT_OPID_T opId)
{
	enum step_trigger_point_id tp_id = STEP_TRIGGER_POINT_NO_DEFINE;

	if (type < WMTDRV_TYPE_BT || type >= WMTDRV_TYPE_MAX) {
		WMT_ERR_FUNC("STEP failed: Do actions from type: %d\n", type);
		return;
	}

	switch (opId) {
	case WMT_OPID_FUNC_OFF:
		tp_id = wmt_step_func_ctrl_id[type][0];
		break;
	case WMT_OPID_FUNC_ON:
		tp_id = wmt_step_func_ctrl_id[type][1];
		break;
	default:
		break;
	}

	if (tp_id != STEP_TRIGGER_POINT_NO_DEFINE) {
		/* default value is 0*/
		wmt_step_do_actions(tp_id);
	}
}

void wmt_step_command_timeout_do_actions(char *reason)
{
	wmt_step_do_actions_from_tp(STEP_TRIGGER_POINT_COMMAND_TIMEOUT, reason);
}

