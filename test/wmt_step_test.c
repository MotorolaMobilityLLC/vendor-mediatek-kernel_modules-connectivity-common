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

#include <linux/platform_device.h>
#include "osal.h"
#include "wmt_step_test.h"
#include "wmt_step.h"
#include "wmt_exp.h"
#include "wmt_lib.h"
#include "mtk_wcn_consys_hw.h"
#include "stp_core.h"
#include "wmt_dbg.h"

struct step_test_check g_step_test_check;

void wmt_step_test_clear_parameter(char *params[])
{
	int i = 0;

	for (i = 0; i < STEP_TEST_ACTION_PARAMETER_NUMBER; i++)
		params[i] = "";
}

#define index_of_array(current_addr, base_addr, type) \
	(((unsigned long)current_addr - (unsigned long)base_addr) / sizeof(type))

int wmt_step_test_check_write_tp(struct step_action_list *p_act_list, enum step_action_id act_id,
	char *params[])
{
	int index = g_step_test_check.step_check_index;
	int i;
	int tp_id;

	if (g_step_test_check.step_check_result == TEST_FAIL)
		return 0;

	g_step_test_check.step_check_index++;

	if (g_step_test_check.step_check_test_tp_id[index] != -1) {
		tp_id = index_of_array(p_act_list, &g_step_env.actions, struct step_action_list);
		if (tp_id != g_step_test_check.step_check_test_tp_id[index]) {
			g_step_test_check.step_check_result = TEST_FAIL;
			WMT_ERR_FUNC("STEP failed: tp_id %d: expect %d(%d)\n", tp_id,
				g_step_test_check.step_check_test_tp_id[index], index);
			return 0;
		}
	}

	if (act_id != g_step_test_check.step_check_test_act_id[index]) {
		g_step_test_check.step_check_result = TEST_FAIL;
		WMT_ERR_FUNC("STEP failed: act_id %d: expect %d(%d)\n", act_id,
			g_step_test_check.step_check_test_tp_id[index], index);
		return 0;
	}

	for (i = 0; i < STEP_TEST_ACTION_PARAMETER_NUMBER; i++) {
		if (osal_strcmp(g_step_test_check.step_check_params[index][0], "") == 0)
			break;

		if (osal_strcmp(params[0], g_step_test_check.step_check_params[index][0]) != 0) {
			g_step_test_check.step_check_result = TEST_FAIL;
			WMT_ERR_FUNC("STEP failed: params[%d] %s: expect %s(%d)\n", i, params[0],
				g_step_test_check.step_check_params[index][0], index);
			return 0;
		}
	}

	g_step_test_check.step_check_result = TEST_PASS;

	return 0;
}

int wmt_step_test_check_create_emi(struct step_emi_action *p_emi_act, int check_params[],
	char *err_result)
{
	int result = TEST_FAIL;

	if (p_emi_act->base.action_id != STEP_ACTION_INDEX_EMI) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_emi_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_emi_act->is_write != check_params[0] ||
		p_emi_act->begin_offset != check_params[1] ||
		p_emi_act->end_offset != check_params[2]) {
		WMT_ERR_FUNC("%s, emi params: %d, %d, %d\n",
			err_result, p_emi_act->is_write, p_emi_act->begin_offset,
			p_emi_act->end_offset);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_reg(struct step_register_action *p_reg_act, int check_params[],
	char *err_result)
{
	int result = TEST_FAIL;

	if (p_reg_act->base.action_id != STEP_ACTION_INDEX_REGISTER) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_reg_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_reg_act->is_write != check_params[0]) {
		WMT_ERR_FUNC("%s, write failed: %d", err_result, p_reg_act->is_write);
		result = TEST_FAIL;
	} else {
		if (p_reg_act->is_write == 0) {
			if (p_reg_act->address != check_params[1] ||
				p_reg_act->offset != check_params[2] ||
				p_reg_act->times != check_params[3] ||
				p_reg_act->delay_time != check_params[4]) {
				WMT_ERR_FUNC(
					"%s, reg params: %d, %p, %d, %d, %d\n",
					err_result, p_reg_act->is_write, p_reg_act->address,
					p_reg_act->offset, p_reg_act->times, p_reg_act->delay_time);
				result = TEST_FAIL;
			} else {
				result = TEST_PASS;
			}
		} else {
			if (p_reg_act->address != check_params[1] ||
				p_reg_act->offset != check_params[2] ||
				p_reg_act->value != check_params[3]) {
				WMT_ERR_FUNC(
					"%s, reg params: %d, %p, %d, %d\n",
					err_result, p_reg_act->is_write, p_reg_act->address,
					p_reg_act->offset, p_reg_act->value);
				result = TEST_FAIL;
			} else {
				result = TEST_PASS;
			}
		}
	}

	return result;
}

int wmt_step_test_check_create_gpio(struct step_gpio_action *p_gpio_act, int check_params[],
	char *err_result)
{
	int result = TEST_FAIL;

	if (p_gpio_act->base.action_id != STEP_ACTION_INDEX_GPIO) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_gpio_act->base.action_id);
		result = TEST_FAIL;
	} else if (p_gpio_act->is_write != check_params[0]) {
		WMT_ERR_FUNC("%s, write failed: %d", err_result, p_gpio_act->is_write);
		result = TEST_FAIL;
	} else {
		if (p_gpio_act->pin_symbol != check_params[1]) {
			WMT_ERR_FUNC("%s, gpio params: %d, %d\n",
			err_result, p_gpio_act->is_write, p_gpio_act->pin_symbol);
			result = TEST_FAIL;
		} else {
			result = TEST_PASS;
		}
	}

	return result;
}

int wmt_step_test_check_create_drst(struct step_disable_reset_action *p_drst_act, int check_params[],
	char *err_result)
{
	int result = TEST_FAIL;

	if (p_drst_act->base.action_id != STEP_ACTION_INDEX_DISABLE_RESET) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_drst_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_crst(struct step_chip_reset_action *p_crst_act, int check_params[],
	char *err_result)
{
	int result = TEST_FAIL;

	if (p_crst_act->base.action_id != STEP_ACTION_INDEX_CHIP_RESET) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_crst_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_keep_wakeup(struct step_keep_wakeup_action *p_kwak_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_kwak_act->base.action_id != STEP_ACTION_INDEX_KEEP_WAKEUP) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_kwak_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_cancel_wakeup(struct step_cancel_wakeup_action *p_cwak_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_cwak_act->base.action_id != STEP_ACTION_INDEX_CANCEL_WAKEUP) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_cwak_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

int wmt_step_test_check_create_periodic_dump(struct step_periodic_dump_action *p_pd_act,
	int check_params[], char *err_result)
{
	int result = TEST_FAIL;

	if (p_pd_act->base.action_id != STEP_ACTION_INDEX_PERIODIC_DUMP) {
		WMT_ERR_FUNC("%s, id wrong: %d\n", err_result, p_pd_act->base.action_id);
		result = TEST_FAIL;
	} else {
		result = TEST_PASS;
	}

	return result;
}

void wmt_step_test_check_emi_act(unsigned int len, ...)
{
	unsigned int offset;
	unsigned int check_result;
	unsigned int value;
	PUINT8 p_virtual_addr = NULL;
	va_list args;

	if (g_step_test_check.step_check_result == TEST_FAIL)
		return;

	va_start(args, len);
	value = va_arg(args, unsigned int);
	offset = g_step_test_check.step_check_emi_offset[g_step_test_check.step_check_index];
	p_virtual_addr = wmt_plat_get_emi_virt_add(offset);
	if (!p_virtual_addr) {
		g_step_test_check.step_check_result = TEST_FAIL;
		WMT_ERR_FUNC("STEP failed: p_virtual_addr offset(%d) is null", offset);
		return;
	}
	check_result = CONSYS_REG_READ(p_virtual_addr);

	if (check_result == value) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else {
		WMT_ERR_FUNC("STEP failed: Value is %d, expect %d", value, check_result);
		g_step_test_check.step_check_result = TEST_FAIL;
	}

	g_step_test_check.step_check_index++;
	va_end(args);
}

void wmt_step_test_check_reg_read_act(unsigned int len, ...)
{
	unsigned int check_result;
	unsigned int value;
	va_list args;

	va_start(args, len);
	value = va_arg(args, unsigned int);
	check_result = CONSYS_REG_READ(g_step_test_check.step_check_register_addr);

	if (check_result == value) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else {
		WMT_ERR_FUNC("STEP failed: Value is %d, expect %d(0x%08x)", value, check_result,
			g_step_test_check.step_check_register_addr);
		g_step_test_check.step_check_result = TEST_FAIL;
	}

	va_end(args);
}

void wmt_step_test_check_reg_write_act(unsigned int len, ...)
{
	unsigned int value;
	va_list args;

	va_start(args, len);
	value = va_arg(args, unsigned int);

	if (g_step_test_check.step_check_write_value == value || value == 0xdeadfeed) {
		g_step_test_check.step_check_result = TEST_PASS;
	} else {
		WMT_ERR_FUNC("STEP failed: Value is %d, expect %d", value,
			g_step_test_check.step_check_write_value);
		g_step_test_check.step_check_result = TEST_FAIL;
	}

	va_end(args);
}


void wmt_step_test_clear_check_data(void)
{
	unsigned int i = 0, j = 0;

	for (i = 0; i < STEP_TEST_ACTION_NUMBER; i++) {
		g_step_test_check.step_check_test_tp_id[i] = 0;
		g_step_test_check.step_check_test_act_id[i] = 0;
		g_step_test_check.step_check_emi_offset[i] = 0;
		for (j = 0; j < STEP_TEST_ACTION_PARAMETER_NUMBER; j++)
			g_step_test_check.step_check_params[i][j] = "";
	}

	g_step_test_check.step_check_total = 0;
	g_step_test_check.step_check_index = 0;
	g_step_test_check.step_check_result = TEST_PASS;
	g_step_test_check.step_check_register_addr = 0;
}

void wmt_step_test_update_result(int result, struct step_test_report *p_report, char *err_result)
{
	if (result != TEST_FAIL) {
		p_report->pass++;
	} else {
		WMT_ERR_FUNC("%s", err_result);
		p_report->fail++;
	}
}

void wmt_step_test_update_result_report(struct step_test_report *p_dest_report,
	struct step_test_report *p_src_report)
{
	p_dest_report->pass += p_src_report->pass;
	p_dest_report->fail += p_src_report->fail;
	p_dest_report->check += p_src_report->check;
}

void wmt_step_test_show_result_report(char *test_name, struct step_test_report *p_report, int sec_begin, int usec_begin,
	int sec_end, int usec_end)
{
	unsigned int total = 0;
	unsigned int pass = 0;
	unsigned int fail = 0;
	unsigned int check = 0;
	int sec = 0;
	int usec = 0;

	pass = p_report->pass;
	fail = p_report->fail;
	check = p_report->check;

	if (usec_end >= usec_begin) {
		sec = sec_end - sec_begin;
		usec = usec_end - usec_begin;
	} else {
		sec = sec_end - sec_begin - 1;
		usec = usec_end - usec_begin + 1000000;
	}

	total = pass + fail + check;
	WMT_INFO_FUNC("%s Total: %d, PASS: %d, FAIL: %d, CHECK: %d, Spend Time: %d.%.6d\n",
		test_name, total, pass, fail, check, sec, usec);
}

void __wmt_step_test_parse_data(const char *buf, struct step_test_report *p_report, char *err_result)
{
	wmt_step_parse_data(buf, osal_strlen((char *)buf), wmt_step_test_check_write_tp);
	if (g_step_test_check.step_check_total != g_step_test_check.step_check_index) {
		WMT_ERR_FUNC("STEP failed: index %d: expect total %d\n", g_step_test_check.step_check_index,
				g_step_test_check.step_check_total);
		g_step_test_check.step_check_result = TEST_FAIL;
	}
	wmt_step_test_update_result(g_step_test_check.step_check_result, p_report, err_result);
}

void __wmt_step_test_create_action(enum step_action_id act_id, char *params[], int result_of_action,
	int check_params[], struct step_test_report *p_report, char *err_result)
{
	struct step_action *p_act = NULL;
	int result = TEST_FAIL;

	p_act = wmt_step_create_action(act_id, params);
	if (p_act != NULL) {
		switch (p_act->action_id) {
		case STEP_ACTION_INDEX_EMI:
			{
				struct step_emi_action *p_emi_act = NULL;

				p_emi_act = (struct step_emi_action *) list_entry_emi_action(p_act);
				result = wmt_step_test_check_create_emi(p_emi_act, check_params,
					err_result);
			}
			break;
		case STEP_ACTION_INDEX_REGISTER:
			{
				struct step_register_action *p_reg_act = NULL;

				p_reg_act =
					(struct step_register_action *) list_entry_register_action(p_act);
				result = wmt_step_test_check_create_reg(p_reg_act, check_params,
					err_result);
			}
			break;
		case STEP_ACTION_INDEX_GPIO:
			{
				struct step_gpio_action *p_gpio_act = NULL;

				p_gpio_act = (struct step_gpio_action *) list_entry_gpio_action(p_act);
				result = wmt_step_test_check_create_gpio(p_gpio_act, check_params,
					err_result);
			}
			break;
		case STEP_ACTION_INDEX_DISABLE_RESET:
			{
				struct step_disable_reset_action *p_drst_act = NULL;

				p_drst_act =
					(struct step_disable_reset_action *) list_entry_drst_action(p_act);
				result = wmt_step_test_check_create_drst(p_drst_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_CHIP_RESET:
			{
				struct step_chip_reset_action *p_crst_act = NULL;

				p_crst_act =
					(struct step_chip_reset_action *) list_entry_crst_action(p_act);
				result = wmt_step_test_check_create_crst(p_crst_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_KEEP_WAKEUP:
			{
				struct step_keep_wakeup_action *p_kwak_act = NULL;

				p_kwak_act =
					(struct step_keep_wakeup_action *) list_entry_kwak_action(p_act);
				result = wmt_step_test_check_create_keep_wakeup(p_kwak_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_CANCEL_WAKEUP:
			{
				struct step_cancel_wakeup_action *p_cwak_act = NULL;

				p_cwak_act =
					(struct step_cancel_wakeup_action *) list_entry_cwak_action(p_act);
				result = wmt_step_test_check_create_cancel_wakeup(p_cwak_act,
					check_params, err_result);
			}
			break;
		case STEP_ACTION_INDEX_PERIODIC_DUMP:
			{
				struct step_periodic_dump_action *p_pd_act = NULL;

				p_pd_act =
					(struct step_periodic_dump_action *) list_entry_pd_action(p_act);
				result = wmt_step_test_check_create_periodic_dump(p_pd_act,
					check_params, err_result);
			}
			break;
		default:
			result = TEST_FAIL;
			break;
		}

		if (result_of_action == -1)
			result = TEST_FAIL;

		wmt_step_remove_action(p_act);
	} else {
		if (result_of_action == -1)
			result = TEST_PASS;
		else
			result = TEST_FAIL;
	}
	wmt_step_test_update_result(result, p_report, err_result);
}

void __wmt_step_test_do_emi_action(enum step_action_id act_id, char *params[], int result_of_action,
	struct step_test_report *p_report, char *err_result)
{
	struct step_action *p_act = NULL;

	p_act = wmt_step_create_action(act_id, params);
	if (p_act != NULL) {
		if (wmt_step_do_emi_action(p_act, wmt_step_test_check_emi_act) ==
			result_of_action) {
			if (g_step_test_check.step_check_total != g_step_test_check.step_check_index) {
				p_report->fail++;
				WMT_ERR_FUNC("%s, index %d: expect total %d\n", err_result,
					g_step_test_check.step_check_index,	g_step_test_check.step_check_total);
			} else if (g_step_test_check.step_check_result == TEST_PASS) {
				p_report->pass++;
			} else {
				p_report->fail++;
				WMT_ERR_FUNC("%s\n", err_result);
			}
		} else {
			WMT_ERR_FUNC("%s, expect result is %d\n", err_result, result_of_action);
			p_report->fail++;
		}
		wmt_step_remove_action(p_act);
	}
}

void __wmt_step_test_do_register_action(enum step_action_id act_id, char *params[], int result_of_action,
	struct step_test_report *p_report, char *err_result)
{
	struct step_action *p_act = NULL;
	unsigned int value = 0;

	p_act = wmt_step_create_action(act_id, params);
	if (p_act != NULL) {
		if (osal_strcmp(params[0], "1") == 0) {
			/* Write register test */
			value = CONSYS_REG_READ(g_step_test_check.step_check_register_addr);
			if (wmt_step_do_register_action(p_act, wmt_step_test_check_reg_write_act) ==
				result_of_action) {
				if (g_step_test_check.step_check_result == TEST_PASS) {
					p_report->pass++;
				} else {
					p_report->fail++;
					WMT_ERR_FUNC("%s\n", err_result);
				}
			} else {
				WMT_ERR_FUNC("%s, expect result is %d\n", err_result,
					result_of_action);
				p_report->fail++;
			}
			CONSYS_REG_WRITE(g_step_test_check.step_check_register_addr, value);
		} else {
			/* Read register test */
			if (wmt_step_do_register_action(p_act, wmt_step_test_check_reg_read_act) ==
				result_of_action) {
				if (g_step_test_check.step_check_result == TEST_PASS) {
					p_report->pass++;
				} else {
					p_report->fail++;
					WMT_ERR_FUNC("%s\n", err_result);
				}
			} else {
				WMT_ERR_FUNC("%s, expect result is %d\n", err_result,
					result_of_action);
				p_report->fail++;
			}
		}
		wmt_step_remove_action(p_act);
	}
}

void wmt_step_test_all(void)
{
	struct step_test_report report = {0, 0, 0};
	bool is_enable_step = g_step_env.is_enable;
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	report.pass = 0;
	report.fail = 0;
	report.check = 0;

	WMT_INFO_FUNC("STEP test: All start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	g_step_env.is_enable = 0;
	wmt_step_test_read_file(&report);
	g_step_env.is_enable = 1;
	wmt_step_test_create_action(&report);
	wmt_step_test_parse_data(&report);
	wmt_step_test_do_emi_action(&report);
	wmt_step_test_do_register_action(&report);
	wmt_step_test_do_gpio_action(&report);
	wmt_step_test_do_wakeup_action(&report);
	wmt_step_test_create_periodic_dump(&report);

	wmt_step_test_do_chip_reset_action(&report);
	g_step_env.is_enable = is_enable_step;

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: All result",
		&report, sec_begin, usec_begin, sec_end, usec_end);
}

void wmt_step_test_read_file(struct step_test_report *p_report)
{
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	WMT_INFO_FUNC("STEP test: Read file start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/********************************
	 ******** Test case 1 ***********
	 ******* Wrong file name ********
	 ********************************/
	if (-1 == wmt_step_read_file("wmt_failed.cfg")) {
		temp_report.pass++;
	} else {
		WMT_ERR_FUNC("STEP failed: (Read file TC-1) expect no file\n");
		temp_report.fail++;
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Read file result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_parse_data(struct step_test_report *p_report)
{
	char *buf = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	WMT_INFO_FUNC("STEP test: Parse data start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/********************************
	 ******** Test case 1 ***********
	 ******** Normal case ***********
	 ********************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();

	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0 0x50 0x9c\r\n"
		"[AT] _REG 0 0x08 5 2\r\n"
		"[AT] DRST\r\n"
		"[AT] _RST\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08 30\r\n"
		"[AT] GPIO 0 8\r\n"
		"[AT] GPIO 1 6 3\r\n"
		"[AT] WAK+\r\n"
		"[AT] WAK-\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 9;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_KEEP_WAKEUP;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params[4][0] = "1";
	g_step_test_check.step_check_params[4][1] = "0x08";
	g_step_test_check.step_check_params[4][2] = "30";
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[5][1] = "8";
	g_step_test_check.step_check_params[6][0] = "1";
	g_step_test_check.step_check_params[6][1] = "6";
	g_step_test_check.step_check_params[6][2] = "3";
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP failed: (Parse data TC-1) Normal case\n");

	/*********************************
	 ******** Test case 2 ************
	 ** Normal case with some space **
	 *********************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI   0  0x50   0x9c \r\n"
		"[AT]    _REG 0   0x08 5   2\r\n"
		"[AT]  DRST\r\n"
		"[AT]    _RST\r\n"
		"  [PD+] 2\r\n"
		"    [AT] _EMI 0 0x50 0x9c\r\n"
		"     [PD-] \r\n"
		" [TP 2] When Firmware trigger assert\r\n"
		"[AT]    _REG   1   0x08  30\r\n"
		"[AT]    GPIO  0  8\r\n"
		" [AT]  GPIO   1  6   3\r\n"
		"  [AT]      WAK+\r\n"
		"  [AT]   WAK-\r\n"
		"  [PD+]     5\r\n"
		"       [AT]    _EMI 0     0x50 0x9c\r\n"
		"  [PD-]   \r\n"
		"\r\n";

	g_step_test_check.step_check_total = 13;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = -1;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[9] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[9] = STEP_ACTION_INDEX_KEEP_WAKEUP;
	g_step_test_check.step_check_test_tp_id[10] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[10] = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	g_step_test_check.step_check_test_tp_id[11] = -1;
	g_step_test_check.step_check_test_act_id[11] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[12] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[12] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params[4][0] = "0";
	g_step_test_check.step_check_params[4][1] = "0x50";
	g_step_test_check.step_check_params[4][2] = "0x9c";
	g_step_test_check.step_check_params[6][0] = "1";
	g_step_test_check.step_check_params[6][1] = "0x08";
	g_step_test_check.step_check_params[6][2] = "30";
	g_step_test_check.step_check_params[7][0] = "0";
	g_step_test_check.step_check_params[7][1] = "8";
	g_step_test_check.step_check_params[8][0] = "1";
	g_step_test_check.step_check_params[8][1] = "6";
	g_step_test_check.step_check_params[8][2] = "3";
	g_step_test_check.step_check_params[11][0] = "0";
	g_step_test_check.step_check_params[11][1] = "0x50";
	g_step_test_check.step_check_params[11][2] = "0x9c";
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP failed: (Parse data TC-2) Normal case with some space\n");

	/***************************************************
	 ****************** Test case 3 ********************
	 ** Failed case not enough parameter (Can parser) **
	 ***************************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0x50 0x9c\r\n"
		"[AT] _REG 0 5 2\r\n"
		"[AT] DRST\r\n"
		"[AT] _RST\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08\r\n"
		"[AT] GPIO 0\r\n"
		"[AT] GPIO 6 3\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 7;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;

	g_step_test_check.step_check_params[0][0] = "0x50";
	g_step_test_check.step_check_params[0][1] = "0x9c";
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "5";
	g_step_test_check.step_check_params[1][2] = "2";
	g_step_test_check.step_check_params[4][0] = "1";
	g_step_test_check.step_check_params[4][1] = "0x08";
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[6][0] = "6";
	g_step_test_check.step_check_params[6][1] = "3";

	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP failed: (Parse data TC-3) Not enough parameter\n");

	/***************************************************
	 ****************** Test case 4 ********************
	 ************** Upcase and lowercase ***************
	 ***************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[Tp 1] When Command timeout\r\n"
		"[At] _emi 0x50 0x9c\r\n"
		"[At] _EMI 0x50 0x9c\r\n"
		"[tP 2] When Firmware trigger assert\r\n"
		"[aT] _reg 1 0x08\r\n"
		"[aT] _REG 1 0x08\r\n"
		"[tp 3] Before Chip reset\r\n"
		"[at] gpio 0\r\n"
		"[at] GPIO 0\r\n"
		"[at] wak+ 0\r\n"
		"[at] wak- 0\r\n"
		"[pd+] 5\r\n"
		"[At] GPIO 0 8\r\n"
		"[pd-]\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 10;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_KEEP_WAKEUP;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	g_step_test_check.step_check_test_tp_id[8] = -1;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[9] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[9] = STEP_ACTION_INDEX_PERIODIC_DUMP;

	g_step_test_check.step_check_params[0][0] = "0x50";
	g_step_test_check.step_check_params[0][1] = "0x9c";
	g_step_test_check.step_check_params[1][0] = "0x50";
	g_step_test_check.step_check_params[1][1] = "0x9c";
	g_step_test_check.step_check_params[2][0] = "1";
	g_step_test_check.step_check_params[2][1] = "0x08";
	g_step_test_check.step_check_params[3][0] = "1";
	g_step_test_check.step_check_params[3][1] = "0x08";
	g_step_test_check.step_check_params[4][0] = "0";
	g_step_test_check.step_check_params[4][1] = "8";
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[5][1] = "8";
	g_step_test_check.step_check_params[8][0] = "0";
	g_step_test_check.step_check_params[8][1] = "8";

	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP failed: (Parse data TC-4) Upcase and lowercase\n");

	/*************************************************
	 ****************** Test case 5 ******************
	 ************** TP sequence switch ***************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08 30\r\n"
		"[AT] GPIO 0 8\r\n"
		"[AT] GPIO 1 6 3\r\n"
		"[tp 3] Before Chip reset\r\n"
		"[AT] DRST\r\n"
		"[AT] _RST\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0 0x50 0x9c\r\n"
		"[AT] _REG 0 0x08 5 2\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 7;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_BEFORE_CHIP_RESET;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_params[0][0] = "1";
	g_step_test_check.step_check_params[0][1] = "0x08";
	g_step_test_check.step_check_params[0][2] = "30";
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "8";
	g_step_test_check.step_check_params[2][0] = "1";
	g_step_test_check.step_check_params[2][1] = "6";
	g_step_test_check.step_check_params[2][2] = "3";
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[5][1] = "0x50";
	g_step_test_check.step_check_params[5][2] = "0x9c";
	g_step_test_check.step_check_params[6][0] = "0";
	g_step_test_check.step_check_params[6][1] = "0x08";
	g_step_test_check.step_check_params[6][2] = "5";
	g_step_test_check.step_check_params[6][3] = "2";
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP failed: (Parse data TC-5) TP sequence switch\n");

	/*********************************
	 ********* Test case 6 ***********
	 ********* More comment **********
	 *********************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();

	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[AT] _EMI 0 0x50 0x9c // show emi 0x50~0x9c\r\n"
		"// show cregister\r\n"
		"[AT] _REG 0 0x08 5 2\r\n"
		"// Do some action\r\n"
		"[AT] DRST // just do it\r\n"
		"[AT] _RST // is ok?\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08 30\r\n"
		"[AT] GPIO 0 8\r\n"
		"[AT] GPIO 1 6 3\r\n"
		"[PD+] 5 // pd start\r\n"
		"[AT] GPIO 0 8 // just do it\r\n"
		"// Do some action\r\n"
		"[PD-] // pd ned\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 9;
	g_step_test_check.step_check_test_tp_id[0] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[6] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[7] = -1;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params[4][0] = "1";
	g_step_test_check.step_check_params[4][1] = "0x08";
	g_step_test_check.step_check_params[4][2] = "30";
	g_step_test_check.step_check_params[5][0] = "0";
	g_step_test_check.step_check_params[5][1] = "8";
	g_step_test_check.step_check_params[6][0] = "1";
	g_step_test_check.step_check_params[6][1] = "6";
	g_step_test_check.step_check_params[6][2] = "3";
	g_step_test_check.step_check_params[7][0] = "0";
	g_step_test_check.step_check_params[7][1] = "8";
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP failed: (Parse data TC-6) More comment\n");

	/*********************************
	 ********* Test case 7 ***********
	 ********* Wrong format **********
	 *********************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();

	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP adfacdadf]When Command timeout\r\n"
		"[AT] _EMI 0 0x50 0x9c\r\n"
		"[TP1]When Command timeout\r\n"
		"[AT] DRST\r\n"
		"[TP-1]When Command timeout\r\n"
		"[AT] _RST\r\n"
		"[T P 2] When Firmware trigger assert\r\n"
		"[AT] WAK+\r\n"
		"[TP 2 When Firmware trigger assert\r\n"
		"[AT] WAK+\r\n"
		"[PD+]\r\n"
		"[PD-]\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT]_REG 1 0x08 30\r\n"
		"[A  T] GPIO 0 8\r\n"
		"[ AT ] GPIO 1 6 3\r\n"
		"AT GPIO 0 8\r\n"
		"[AT WAK+\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 0;
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP failed: (Parse data TC-7) Wrong format\n");

	/********************************
	 ******** Test case 8 ***********
	 ******* Periodic dump **********
	 ********************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_check_data();

	buf =
		"// TEST NOW\r\n"
		"\r\n"
		"[TP 1] When Command timeout\r\n"
		"[PD+] 5\r\n"
		"[AT] _EMI 0 0x50 0x9c\r\n"
		"[AT] _REG 0 0x08 5 2\r\n"
		"[PD-]\r\n"
		"[AT] DRST\r\n"
		"[AT] _RST\r\n"
		"[TP 2] When Firmware trigger assert\r\n"
		"[AT] _REG 1 0x08 30\r\n"
		"[PD+] 3\r\n"
		"[AT] GPIO 0 8\r\n"
		"[PD-]\r\n"
		"[AT] GPIO 1 6 3\r\n"
		"[AT] WAK+\r\n"
		"[AT] WAK-\r\n"
		"\r\n";

	g_step_test_check.step_check_total = 11;
	g_step_test_check.step_check_test_tp_id[0] = -1;
	g_step_test_check.step_check_test_act_id[0] = STEP_ACTION_INDEX_EMI;
	g_step_test_check.step_check_test_tp_id[1] = -1;
	g_step_test_check.step_check_test_act_id[1] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[2] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[2] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_test_tp_id[3] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[3] = STEP_ACTION_INDEX_DISABLE_RESET;
	g_step_test_check.step_check_test_tp_id[4] = STEP_TRIGGER_POINT_COMMAND_TIMEOUT;
	g_step_test_check.step_check_test_act_id[4] = STEP_ACTION_INDEX_CHIP_RESET;
	g_step_test_check.step_check_test_tp_id[5] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[5] = STEP_ACTION_INDEX_REGISTER;
	g_step_test_check.step_check_test_tp_id[6] = -1;
	g_step_test_check.step_check_test_act_id[6] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[7] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[7] = STEP_ACTION_INDEX_PERIODIC_DUMP;
	g_step_test_check.step_check_test_tp_id[8] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[8] = STEP_ACTION_INDEX_GPIO;
	g_step_test_check.step_check_test_tp_id[9] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[9] = STEP_ACTION_INDEX_KEEP_WAKEUP;
	g_step_test_check.step_check_test_tp_id[10] = STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT;
	g_step_test_check.step_check_test_act_id[10] = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	g_step_test_check.step_check_params[0][0] = "0";
	g_step_test_check.step_check_params[0][1] = "0x50";
	g_step_test_check.step_check_params[0][2] = "0x9c";
	g_step_test_check.step_check_params[1][0] = "0";
	g_step_test_check.step_check_params[1][1] = "0x08";
	g_step_test_check.step_check_params[1][2] = "5";
	g_step_test_check.step_check_params[1][3] = "2";
	g_step_test_check.step_check_params[5][0] = "1";
	g_step_test_check.step_check_params[5][1] = "0x08";
	g_step_test_check.step_check_params[5][2] = "30";
	g_step_test_check.step_check_params[6][0] = "0";
	g_step_test_check.step_check_params[6][1] = "8";
	g_step_test_check.step_check_params[8][0] = "1";
	g_step_test_check.step_check_params[8][1] = "6";
	g_step_test_check.step_check_params[8][2] = "3";
	__wmt_step_test_parse_data(buf, &temp_report,
		"STEP failed: (Parse data TC-8) Periodic dump\n");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Parse data result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_create_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_TEST_ACTION_PARAMETER_NUMBER];
	int check_params[STEP_TEST_ACTION_PARAMETER_NUMBER];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	struct step_pd_entry fack_pd_entry;

	WMT_INFO_FUNC("STEP test: Create action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/*****************************
	 ******** Test case 1 ********
	 **** EMI create for read ****
	 *****************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "0";
	params[1] = "0x50";
	params[2] = "0x9c";
	check_params[0] = 0;
	check_params[1] = 0x50;
	check_params[2] = 0x9c;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-1) EMI create");

	/****************************************
	 ************ Test case 2 ***************
	 **** REGISTER(Addr) create for read ****
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	params[3] = "2";
	params[4] = "10";
	check_params[0] = 0;
	check_params[1] = 0x124dfad;
	check_params[2] = 0x9c;
	check_params[3] = 2;
	check_params[4] = 10;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-2) EREG create read");

	/*****************************************
	 ************ Test case 3 ****************
	 **** REGISTER(Addr) create for write ****
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "1";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	params[3] = "15";
	check_params[0] = 1;
	check_params[1] = 0x124dfad;
	check_params[2] = 0x9c;
	check_params[3] = 15;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-3) REG create write");

	/******************************************
	 ************ Test case 4 *****************
	 ********** GPIO create for read **********
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_GPIO;
	params[0] = "0";
	params[1] = "8";
	check_params[0] = 0;
	check_params[1] = 8;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-4) GPIO create read");

	/*****************************************
	 ************ Test case 5 ****************
	 ********* DISABLE REST create ***********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_DISABLE_RESET;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-5) DISABLE REST");

	/*****************************************
	 ************ Test case 6 ****************
	 ********** CHIP REST create *************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CHIP_RESET;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-6) CHIP REST");

	/******************************************
	 ************** Test case 7 ***************
	 *********** read wrong symbol ************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	params[1] = "#10000";
	params[2] = "0x204";
	params[3] = "1";
	params[4] = "0";
	check_params[0] = 0;
	check_params[1] = 0x124dfad;
	check_params[2] = 0x9c;
	check_params[3] = 2;
	check_params[4] = 10;
	__wmt_step_test_create_action(act_id, params, -1, check_params, &temp_report,
		"STEP failed: (Create action TC-7) read wrong symbol");

	/*****************************************
	 ************ Test case 8 ****************
	 ******** Keep Wakeup create *************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_KEEP_WAKEUP;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-8) Keep wakeup");

	/*****************************************
	 ************ Test case 9 ****************
	 ***** Cancel keep wakeup create *********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 9\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_CANCEL_WAKEUP;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-9) Cancel keep wakeup");

	/************************************
	 ********** Test case 10 ************
	 **** EMI create fail less param ****
	 ************************************/
	WMT_INFO_FUNC("STEP test: TC 10\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "0";
	params[1] = "0x50";
	__wmt_step_test_create_action(act_id, params, -1, check_params, &temp_report,
		"STEP failed: (Create action TC-10) EMI create fail");

	/****************************************************
	 **************** Test case 11 **********************
	 **** REGISTER(Addr) create read fail less param ****
	 ****************************************************/
	WMT_INFO_FUNC("STEP test: TC 11\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	__wmt_step_test_create_action(act_id, params, -1, check_params, &temp_report,
		"STEP failed: (Create action TC-11) REG create read fail");

	/*****************************************************
	 ************ Test case 12 ***************************
	 **** REGISTER(Addr) create write fail less param ****
	 *****************************************************/
	WMT_INFO_FUNC("STEP test: TC 12\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "1";
	params[1] = "0x124dfad";
	params[2] = "0x9c";
	__wmt_step_test_create_action(act_id, params, -1, check_params, &temp_report,
		"STEP failed: (Create action TC-12) REG create write fail");

	/*************************************************
	 **************** Test case 13 *******************
	 ********** GPIO create fail less param **********
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 13\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_GPIO;
	params[0] = "0";
	__wmt_step_test_create_action(act_id, params, -1, check_params, &temp_report,
		"STEP failed: (Create action TC-13) GPIO create fail");

	/*************************************************
	 **************** Test case 14 *******************
	 ************** Periodic dump create *************
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 14\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_PERIODIC_DUMP;
	params[0] = (PINT8)&fack_pd_entry;
	__wmt_step_test_create_action(act_id, params, 0, check_params, &temp_report,
		"STEP failed: (Create action TC-14) Periodic dump create fail");

	/*************************************************
	 **************** Test case 15 *******************
	 ****** Periodic dump create fail no param *******
	 *************************************************/
	WMT_INFO_FUNC("STEP test: TC 15\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_PERIODIC_DUMP;
	params[0] = NULL;
	__wmt_step_test_create_action(act_id, params, -1, check_params, &temp_report,
		"STEP failed: (Create action TC-15) Periodic dump create fail");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Create action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_get_emi_wmt_offset(unsigned char buf[], int offset)
{
	snprintf(buf, 11, "0x%08x", ((unsigned int)STEP_TEST_CONSYS_EMI_WMT_OFFSET + offset));
}

void wmt_step_test_do_emi_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_TEST_ACTION_PARAMETER_NUMBER];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	unsigned char buf_begin[11];
	unsigned char buf_end[11];

	WMT_INFO_FUNC("STEP test: Do EMI action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/*****************************************
	 ************ Test case 1 ****************
	 ********** EMI dump 32 bit **************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x44);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x48);
	params[2] = buf_end;
	g_step_test_check.step_check_total = 1;
	g_step_test_check.step_check_emi_offset[0] = 0x44;
	__wmt_step_test_do_emi_action(act_id, params, 0, &temp_report,
		"STEP failed: (Do EMI action TC-1) dump 32bit");

	/*****************************************
	 ************ Test case 2 ****************
	 ****** EMI dump check for address *******
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x24);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x44);
	params[2] = buf_end;
	g_step_test_check.step_check_total = 8;
	g_step_test_check.step_check_emi_offset[0] = 0x24;
	g_step_test_check.step_check_emi_offset[1] = 0x28;
	g_step_test_check.step_check_emi_offset[2] = 0x2c;
	g_step_test_check.step_check_emi_offset[3] = 0x30;
	g_step_test_check.step_check_emi_offset[4] = 0x34;
	g_step_test_check.step_check_emi_offset[5] = 0x38;
	g_step_test_check.step_check_emi_offset[6] = 0x3c;
	g_step_test_check.step_check_emi_offset[7] = 0x40;
	__wmt_step_test_do_emi_action(act_id, params, 0, &temp_report,
		"STEP failed: (Do EMI action TC-2) more address");

	/*****************************************
	 ************ Test case 3 ****************
	 **** EMI dump begin larger than end *****
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x20);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x08);
	params[2] = buf_end;
	g_step_test_check.step_check_total = 6;
	g_step_test_check.step_check_emi_offset[0] = 0x08;
	g_step_test_check.step_check_emi_offset[1] = 0x0c;
	g_step_test_check.step_check_emi_offset[2] = 0x10;
	g_step_test_check.step_check_emi_offset[3] = 0x14;
	g_step_test_check.step_check_emi_offset[4] = 0x18;
	g_step_test_check.step_check_emi_offset[5] = 0x1c;
	__wmt_step_test_do_emi_action(act_id, params, 0, &temp_report,
		"STEP failed: (Do EMI action TC-3) begin larger than end");

	/****************************************
	 ************ Test case 4 ***************
	 ******** EMI only support read *********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "1";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x20);
	params[2] = buf_end;
	__wmt_step_test_do_emi_action(act_id, params, -1, &temp_report,
		"STEP failed: (Do EMI action TC-4) only support read");

	/****************************************
	 ************ Test case 5 ***************
	 ********* EMI dump not 32bit ***********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x08);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x0e);
	params[2] = buf_end;
	g_step_test_check.step_check_total = 2;
	g_step_test_check.step_check_emi_offset[0] = 0x08;
	g_step_test_check.step_check_emi_offset[1] = 0x0c;
	__wmt_step_test_do_emi_action(act_id, params, 0, &temp_report,
		"STEP failed: (Do EMI action TC-5) not 32bit");

	/*****************************************
	 ************ Test case 6 ****************
	 ***** EMI dump over emi max size ********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, (gConEmiSize + 0x08));
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, (gConEmiSize + 0x0e));
	params[2] = buf_end;
	__wmt_step_test_do_emi_action(act_id, params, -1, &temp_report,
		"STEP failed: (Do EMI action TC-6) over emi max size");

	/*****************************************
	 ************ Test case 7 ****************
	 ************* page fault ****************
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_EMI;
	params[0] = "0";
	wmt_step_test_get_emi_wmt_offset(buf_begin, 0x02);
	params[1] = buf_begin;
	wmt_step_test_get_emi_wmt_offset(buf_end, 0x08);
	params[2] = buf_end;
	__wmt_step_test_do_emi_action(act_id, params, -1, &temp_report,
		"STEP failed: (Do EMI action TC-7) page fault");

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Do EMI action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

int wmt_step_test_get_reg_base_phy_addr(unsigned char buf[], unsigned int index)
{
	struct device_node *node = NULL;
	struct resource res;
	int ret;

	if (g_pdev != NULL) {
		node = g_pdev->dev.of_node;
		if (node) {
			ret = of_address_to_resource(node, index, &res);
			if (ret) {
				WMT_ERR_FUNC("STEP failed: of_address_to_resource null");
				return ret;
			}
		} else {
			WMT_ERR_FUNC("STEP failed: node null");
			return -1;
		}
	} else {
		WMT_ERR_FUNC("STEP failed: gdev null");
		return -1;
	}
	snprintf(buf, 11, "0x%08x", ((unsigned int)res.start));

	return 0;
}

void wmt_step_test_do_register_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_TEST_ACTION_PARAMETER_NUMBER];
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	unsigned char buf[11];

	WMT_INFO_FUNC("STEP test: Do register action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/****************************************
	 ************ Test case 1 ***************
	 ******** REG read MCU chip id **********
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x08";
	params[3] = "1";
	params[4] = "0";
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
	__wmt_step_test_do_register_action(act_id, params, 0, &temp_report,
		"STEP failed: (Do register action TC-1) MCU chip id");

	/****************************************
	 ************ Test case 2 ***************
	 *** REG read cpucpr 5 times / 200ms ****
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 2\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x160";
	params[3] = "5";
	params[4] = "200";
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x160);
	__wmt_step_test_do_register_action(act_id, params, 0, &temp_report,
		"STEP failed: (Do register action TC-2) cpucpr 5 times / 200ms");

	/**********************************************
	 *************** Test case 3 ******************
	 ** REG read MCU chip id by physical address **
	 **********************************************/
	WMT_INFO_FUNC("STEP test: TC 3\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = "0x08";
		params[3] = "1";
		params[4] = "0";
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x08);
		__wmt_step_test_do_register_action(act_id, params, 0, &temp_report,
			"STEP failed: (Do register action TC-3) MCU chip id by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP failed: get physical address failed\n");
	}

	/*********************************************************
	 ********************* Test case 4 ***********************
	 ** REG read cpucpr 5 times / 200ms by physical address **
	 *********************************************************/
	WMT_INFO_FUNC("STEP test: TC 4\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = "0x160";
		params[3] = "5";
		params[4] = "20";
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x160);
		__wmt_step_test_do_register_action(act_id, params, 0, &temp_report,
			"STEP failed: (Do register action TC-4) cpucpr 5 times / 200ms by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP failed: get physical address failed\n");
	}

	/*****************************************
	 ************* Test case 5 ***************
	 ******** REG read over base size ********
	 *****************************************/
	WMT_INFO_FUNC("STEP test: TC 5\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	params[1] = "#1";
	params[2] = "0x1204";
	params[3] = "1";
	params[4] = "0";
	__wmt_step_test_do_register_action(act_id, params, -1, &temp_report,
		"STEP failed: (Do register action TC-5) Over size");

	/******************************************
	 ************** Test case 6 ***************
	 ***** REG read over base size by phy *****
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 6\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "0";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = "0x204";
		params[3] = "1";
		params[4] = "0";
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x204);
		__wmt_step_test_do_register_action(act_id, params, 0, &temp_report,
			"STEP failed: (Do register action TC-6) Over size by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP failed: get physical address failed\n");
	}

	/******************************************
	 ************** Test case 7 ***************
	 *************** REG write ****************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 7\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "1";
	params[1] = "#1";
	params[2] = "0x110";
	params[3] = "0x100";
	g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x160);
	g_step_test_check.step_check_write_value = 0x100;
	__wmt_step_test_do_register_action(act_id, params, 0, &temp_report,
		"STEP failed: (Do register action TC-7) REG write");

	/******************************************
	 ************** Test case 8 ***************
	 *********** REG write by phy *************
	 ******************************************/
	WMT_INFO_FUNC("STEP test: TC 8\n");
	wmt_step_test_clear_check_data();
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_REGISTER;
	params[0] = "1";
	if (wmt_step_test_get_reg_base_phy_addr(buf, 0) == 0) {
		params[1] = buf;
		params[2] = "0x110";
		params[3] = "0x000";
		g_step_test_check.step_check_register_addr = (conn_reg.mcu_base + 0x160);
		g_step_test_check.step_check_write_value = 0x0;
		__wmt_step_test_do_register_action(act_id, params, 0, &temp_report,
			"STEP failed: (Do register action TC-8) REG write by phy");
	} else {
		p_report->fail++;
		WMT_ERR_FUNC("STEP failed: get physical address failed\n");
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Do register action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_gpio_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_TEST_ACTION_PARAMETER_NUMBER];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	WMT_INFO_FUNC("STEP test: Do GPIO action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/****************************************
	 ************* Test case 1 **************
	 ************* GPIO read #8 *************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	wmt_step_test_clear_parameter(params);
	act_id = STEP_ACTION_INDEX_GPIO;
	params[0] = "0";
	params[1] = "8";

	p_act = wmt_step_create_action(act_id, params);
	if (p_act != NULL) {
		if (wmt_step_do_gpio_action(p_act, NULL) == 0) {
			WMT_INFO_FUNC("STEP check: Do gpio action TC-1(Read #8): search(8: )");
			temp_report.check++;
		} else {
			WMT_ERR_FUNC("STEP failed: (Do gpio action TC-1) Read #8\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Do GPIO action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_chip_reset_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_TEST_ACTION_PARAMETER_NUMBER];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	WMT_INFO_FUNC("STEP test: Do chip reset action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/****************************************
	 ************* Test case 1 **************
	 ************* chip reset ***************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	act_id = STEP_ACTION_INDEX_CHIP_RESET;

	p_act = wmt_step_create_action(act_id, params);
	if (p_act != NULL) {
		if (wmt_step_do_chip_reset_action(p_act, NULL) == 0) {
			WMT_INFO_FUNC("STEP check: Do chip reset TC-1(chip reset): Trigger AEE");
			temp_report.check++;
		} else {
			WMT_ERR_FUNC("STEP failed: (Do chip reset action TC-1) chip reset\n");
			temp_report.fail++;
		}
		wmt_step_remove_action(p_act);
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Do chip reset action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_do_wakeup_action(struct step_test_report *p_report)
{
	enum step_action_id act_id;
	char *params[STEP_TEST_ACTION_PARAMETER_NUMBER];
	struct step_action *p_act = NULL;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;

	WMT_INFO_FUNC("STEP test: Do wakeup action start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);
	/****************************************
	 ************* Test case 1 **************
	 ***** Wakeup then read/write reg *******
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	act_id = STEP_ACTION_INDEX_KEEP_WAKEUP;

	p_act = wmt_step_create_action(act_id, params);
	if (p_act != NULL) {
		wmt_step_do_keep_wakeup_action(p_act, NULL);
		wmt_step_test_do_register_action(&temp_report);
		wmt_step_remove_action(p_act);
	}

	act_id = STEP_ACTION_INDEX_CANCEL_WAKEUP;

	p_act = wmt_step_create_action(act_id, params);
	if (p_act != NULL) {
		wmt_step_do_cancel_wakeup_action(p_act, NULL);
		wmt_step_remove_action(p_act);
	}

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Do wakeup action result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

void wmt_step_test_create_periodic_dump(struct step_test_report *p_report)
{
	int expires_ms;
	struct step_test_report temp_report = {0, 0, 0};
	int sec_begin = 0;
	int usec_begin = 0;
	int sec_end = 0;
	int usec_end = 0;
	struct step_pd_entry *p_current;
	bool is_thread_run_for_test = 0;

	WMT_INFO_FUNC("STEP test: Create periodic dump start\n");
	osal_gettimeofday(&sec_begin, &usec_begin);

	if (g_step_env.pd_struct.step_pd_wq == NULL) {
		if (wmt_step_init_pd_env() != 0) {
			WMT_ERR_FUNC("STEP failed: Start thread failed\n");
			return;
		}
		is_thread_run_for_test = 1;
	}

	/****************************************
	 ************* Test case 1 **************
	 *************** Normal *****************
	 ****************************************/
	WMT_INFO_FUNC("STEP test: TC 1\n");
	expires_ms = 5;
	p_current = wmt_step_get_periodic_dump_entry(expires_ms);
	if (p_current == NULL) {
		WMT_ERR_FUNC("STEP failed: (Create periodic dump TC-1) No entry\n");
		temp_report.fail++;
	} else {
		if (p_current->expires_ms == expires_ms) {
			temp_report.pass++;
		} else {
			WMT_ERR_FUNC("STEP failed: (Create periodic dump TC-1) Currect %d not %d\n",
				p_current->expires_ms, expires_ms);
			temp_report.fail++;
		}
		list_del_init(&p_current->list);
		kfree(p_current);
	}

	if (is_thread_run_for_test == 1)
		wmt_step_deinit_pd_env();

	osal_gettimeofday(&sec_end, &usec_end);
	wmt_step_test_show_result_report("STEP test: Create periodic dump result",
		&temp_report, sec_begin, usec_begin, sec_end, usec_end);
	wmt_step_test_update_result_report(p_report, &temp_report);
}

