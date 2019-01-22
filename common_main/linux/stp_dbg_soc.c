/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "stp_dbg.h"
#include "stp_dbg_soc.h"
#include "btm_core.h"
#include "stp_core.h"
#include "mtk_wcn_consys_hw.h"
#include "wmt_step.h"
#include <linux/ratelimit.h>

#define STP_DBG_PAGED_TRACE_SIZE (2048*sizeof(char))
#define SUB_PKT_SIZE 1024
#define SUB_PKT_HEADER 5
#define EMI_SYNC_TIMEOUT 100 /* FW guarantee that MCU copy data time is ~20ms. We set 100ms for safety */
#define IS_VISIBLE_CHAR(c) ((c) >= 32 && (c) <= 126)
#define DUMP_LOG_BYTES_PER_LINE (128)

ENUM_STP_FW_ISSUE_TYPE issue_type;
UINT8 g_paged_trace_buffer[STP_DBG_PAGED_TRACE_SIZE] = { 0 };
UINT32 g_paged_trace_len;
UINT8 g_paged_dump_buffer[STP_DBG_PAGED_DUMP_BUFFER_SIZE] = { 0 };
UINT32 g_paged_dump_len;

static PUINT8 soc_task_str[SOC_TASK_ID_INDX_MAX][SOC_GEN3_TASK_ID_MAX] = {
	{"Task_WMT",
	"Task_BT",
	"Task_Wifi",
	"Task_Tst",
	"Task_FM",
	"Task_Idle",
	"Task_DrvStp",
	"Task_DrvBtif",
	"Task_NatBt",
	"Task_DrvWifi"},
	{"Task_WMT",
	"Task_BT",
	"Task_Wifi",
	"Task_Tst",
	"Task_FM",
	"Task_GPS",
	"Task_FLP",
	"Task_BT2",
	"Task_Idle",
	"Task_DrvStp",
	"Task_DrvBtif",
	"Task_NatBt",
	"Task_DrvWifi"},
};

static _osal_inline_ INT32 stp_dbg_soc_paged_dump(INT32 dump_sink);
static _osal_inline_ INT32 stp_dbg_soc_paged_trace(VOID);
static _osal_inline_ INT32 stp_dbg_soc_put_emi_dump_to_nl(PUINT8 data_buf, INT32 dump_len);
static _osal_inline_ VOID stp_dbg_soc_emi_dump_buffer(UINT8 *buffer, UINT32 len);

static VOID stp_dbg_dump_log(PUINT8 buf, INT32 size)
{
	INT32 i = 0;
	UINT8 line[DUMP_LOG_BYTES_PER_LINE + 1];

	while (size--) {
		if (IS_VISIBLE_CHAR(*buf))
			line[i] = *buf;
		else
			line[i] = '.';
		i++;
		buf++;

		if (i >= DUMP_LOG_BYTES_PER_LINE || !size) {
			line[i] = 0;
			pr_info("page_trace: %s\n", line);
			i = 0;
		}
	}
}

static _osal_inline_ VOID stp_dbg_soc_emi_dump_buffer(UINT8 *buffer, UINT32 len)
{
	UINT32 i = 0;

	if (len > 16)
		len = 16;
	for (i = 0; i < len; i++) {
		if (i % 16 == 0 && i != 0)
			pr_cont("\n    ");

		if (buffer[i] == ']' || buffer[i] == '[' || buffer[i] == ',')
			pr_cont("%c", buffer[i]);
		else
			pr_cont("0x%02x ", buffer[i]);
	}
}

static _osal_inline_ INT32 stp_dbg_soc_put_emi_dump_to_nl(PUINT8 data_buf, INT32 dump_len)
{
	static UINT8  tmp[SUB_PKT_SIZE + SUB_PKT_HEADER];
	INT32 remain = dump_len, index = 0;
	INT32 ret = 0;
	INT32 len;
	INT32 offset = 0;

	STP_DBG_INFO_FUNC("Enter..\n");

	if (dump_len > 0) {
		index = 0;
		tmp[index++] = '[';
		tmp[index++] = 'M';
		tmp[index++] = ']';

		do {
			index = 3;
			if (remain >= SUB_PKT_SIZE)
				len = SUB_PKT_SIZE;
			else
				len = remain;
			remain -= len;

			osal_memcpy(&tmp[index], &len, 2);
			index += 2;
			osal_memcpy(&tmp[index], data_buf + offset, len);
			offset += len;
			STP_DBG_DBG_FUNC("send %d remain %d\n", len, remain);

			ret = stp_dbg_dump_send_retry_handler((PINT8)&tmp, len);
			if (ret)
				break;

			/* schedule(); */
		} while (remain > 0);
	} else
		STP_DBG_INFO_FUNC("dump entry length is 0\n");
	STP_DBG_INFO_FUNC("Exit..\n");

	return ret;
}

static _osal_inline_ UINT64 stp_dbg_soc_elapsed_time(UINT64 ts, ULONG nsec)
{
	UINT64 current_ts = 0;
	ULONG current_nsec = 0;

	osal_get_local_time(&current_ts, &current_nsec);
	return (current_ts*1000 + current_nsec/1000) - (ts*1000 + nsec/1000);
}

static _osal_inline_ INT32 stp_dbg_soc_paged_dump(INT32 dump_sink)
{
	INT32 ret = 0;
	UINT32 counter = 0;
	UINT32 dump_num = 0;
	UINT32 packet_num = STP_PAGED_DUMP_TIME_LIMIT/100;
	UINT32 page_counter = 0;
	ENUM_CHIP_DUMP_STATE chip_state;
	UINT32 dump_phy_addr = 0;
	PUINT8 dump_vir_addr = NULL;
	INT32 dump_len = 0;
	P_CONSYS_EMI_ADDR_INFO p_ecsi;
	UINT64 start_ts = 0;
	ULONG start_nsec = 0;
	UINT64 elapsed_time = 0;
	INT32 abort = 0;
#if WMT_DBG_SUPPORT
	static DEFINE_RATELIMIT_STATE(_rs, 10 * HZ, 1);
#endif

	g_paged_dump_len = 0;
	p_ecsi = wmt_plat_get_emi_phy_add();
	osal_assert(p_ecsi);

	issue_type = STP_FW_ASSERT_ISSUE;
	if (chip_reset_only) {
		STP_DBG_WARN_FUNC("is chip reset only\n");
		ret = -3;
		return ret;
	}

	if (dump_sink == 0)
		return 0;

	/*packet number depend on dump_num get from register:0xf0080044 ,support jade*/
	dump_num = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_page_dump_num);
	if (dump_num != 0) {
		packet_num = dump_num;
		STP_DBG_WARN_FUNC("get consys dump num packet_num(%d)\n", packet_num);
	} else {
		dump_num = CORE_DUMP_NUM;
		STP_DBG_ERR_FUNC("can not get consys dump num and default num is %d\n", CORE_DUMP_NUM);
	}
	stp_dbg_dump_num(dump_num);

	if (dump_sink == 1 || dump_sink == 2)
		stp_dbg_start_coredump_timer();

	wmt_plat_set_host_dump_state(STP_HOST_DUMP_NOT_START);
	page_counter = 0;
	if (mtk_wcn_stp_get_wmt_trg_assert() == 0)
		WMT_STEP_DO_ACTIONS_FUNC(STEP_TRIGGER_POINT_FIRMWARE_TRIGGER_ASSERT);

	do {
		dump_phy_addr = 0;
		dump_vir_addr = NULL;
		dump_len = 0;

		counter++;
		osal_get_local_time(&start_ts, &start_nsec);
		while (1) {
			elapsed_time = stp_dbg_soc_elapsed_time(start_ts, start_nsec);
			chip_state = (ENUM_CHIP_DUMP_STATE)wmt_plat_get_dump_info(
					p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_state);
			if (chip_state == STP_CHIP_DUMP_PUT_DONE) {
				STP_DBG_INFO_FUNC("chip put done\n");
				break;
			}
			STP_DBG_INFO_FUNC("waiting chip put done, chip_state: %d\n", chip_state);
#if WMT_DBG_SUPPORT
			if (chip_state == 0 && __ratelimit(&_rs))
				stp_dbg_poll_cpupcr(5, 1, 1);
#endif

			if (elapsed_time > EMI_SYNC_TIMEOUT) {
#if !WMT_DBG_SUPPORT
				STP_DBG_ERR_FUNC("Wait Timeout: %llu > %d\n", elapsed_time, EMI_SYNC_TIMEOUT);
				/* Since customer's user/userdebug load get coredump via netlink(dump_sink==2). */
				/* For UX, if get coredump timeout, skip it and do chip reset ASAP. */
				if (dump_sink == 2)
					abort = 1;
#endif
				goto paged_dump_end;
			}
			osal_sleep_ms(1);
		}

		dump_phy_addr = wmt_plat_get_dump_info(
				p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_addr);

		if (!dump_phy_addr) {
			STP_DBG_ERR_FUNC("get paged dump phy address fail\n");
			ret = -1;
			break;
		}

		dump_vir_addr = wmt_plat_get_emi_virt_add(dump_phy_addr - p_ecsi->emi_phy_addr);
		if (!dump_vir_addr) {
			STP_DBG_ERR_FUNC("get paged dump phy address fail\n");
			ret = -2;
			break;
		}
		dump_len = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_len);
		STP_DBG_DBG_FUNC("dump_phy_ddr(%08x),dump_vir_add(0x%p),dump_len(%d)\n",
				dump_phy_addr, dump_vir_addr, dump_len);

		/* dump_len should not be negative */
		if (dump_len < 0)
			dump_len = 0;

		/*move dump info according to dump_addr & dump_len */
		osal_memcpy_fromio(&g_paged_dump_buffer[0], dump_vir_addr, dump_len);

		if (dump_len <= 32 * 1024) {
			pr_err("coredump mode: %d!\n", dump_sink);
			switch (dump_sink) {
			case 0:
				STP_DBG_INFO_FUNC("coredump is disabled!\n");
				return 0;
			case 1:
				ret = stp_dbg_aee_send(&g_paged_dump_buffer[0], dump_len, 0);
				if (ret == 0)
					STP_DBG_INFO_FUNC("aee send ok!\n");
				else if (ret == 1)
					STP_DBG_INFO_FUNC("aee send fisish!\n");
				else if (ret == -1) {
					STP_DBG_ERR_FUNC("aee send timeout!\n");
					abort = 1;
					goto paged_dump_end;
				} else
					STP_DBG_ERR_FUNC("aee send error!\n");
				break;
			case 2:
				ret = stp_dbg_soc_put_emi_dump_to_nl(&g_paged_dump_buffer[0], dump_len);
				if (ret == 0)
					STP_DBG_INFO_FUNC("dump send ok!\n");
				else if (ret == 1) {
					STP_DBG_ERR_FUNC("dump send timeout!\n");
					abort = 1;
					goto paged_dump_end;
				} else
					STP_DBG_ERR_FUNC("dump send error!\n");
				break;
			default:
				STP_DBG_ERR_FUNC("unknown sink %d\n", dump_sink);
				return -1;
			}
		} else
			STP_DBG_ERR_FUNC("dump len is over than 32K(%d)\n", dump_len);

		g_paged_dump_len += dump_len;
		wmt_plat_update_host_sync_num();
		wmt_plat_set_host_dump_state(STP_HOST_DUMP_GET_DONE);

		STP_DBG_INFO_FUNC("host sync num(%d),chip sync num(%d)\n",
				wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_host_sync_num),
				wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_num));
		page_counter++;
		STP_DBG_INFO_FUNC("++ paged dump counter(%d) ++\n", page_counter);
		/* dump 1st 512 bytes data to kernel log for fw requirement */
		if (page_counter == 1)
			stp_dbg_dump_log(&g_paged_dump_buffer[0], dump_len < 512 ? dump_len : 512);

		osal_get_local_time(&start_ts, &start_nsec);
		while (1) {
			elapsed_time = stp_dbg_soc_elapsed_time(start_ts, start_nsec);
			chip_state = (ENUM_CHIP_DUMP_STATE)wmt_plat_get_dump_info(
					p_ecsi->p_ecso->emi_apmem_ctrl_chip_sync_state);
			if (chip_state == STP_CHIP_DUMP_END) {
				STP_DBG_INFO_FUNC("chip put end\n");
				break;
			}
			STP_DBG_INFO_FUNC("waiting chip put end, chip_state: %d\n", chip_state);
			if (elapsed_time > EMI_SYNC_TIMEOUT) {
#if !WMT_DBG_SUPPORT
				STP_DBG_ERR_FUNC("Wait Timeout: %llu > %d\n", elapsed_time, EMI_SYNC_TIMEOUT);
				/* Since customer's user/userdebug load get coredump via netlink(dump_sink==2). */
				/* For UX, if wait sync state timeout, skip it and do chip reset ASAP. */
				if (dump_sink == 2)
					abort = 1;
#endif
				goto paged_dump_end;
			}
			osal_sleep_ms(1);
		}

paged_dump_end:
		wmt_plat_set_host_dump_state(STP_HOST_DUMP_NOT_START);
		STP_DBG_INFO_FUNC("++ counter(%d) packet_num(%d) page_counter(%d) g_paged_dump_len(%d)++\n",
			counter, packet_num, page_counter, g_paged_dump_len);
		if (wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_paded_dump_end)) {
			if (stp_dbg_get_coredump_timer_state() == CORE_DUMP_DOING) {
				STP_DBG_INFO_FUNC("paged dump end by emi flag\n");
				if (dump_sink == 1)
					stp_dbg_aee_send(FAKECOREDUMPEND, osal_sizeof(FAKECOREDUMPEND), 0);
				else if (dump_sink == 2)
					stp_dbg_nl_send_data(FAKECOREDUMPEND, osal_sizeof(FAKECOREDUMPEND));
			} else
				STP_DBG_INFO_FUNC("paged dump end\n");
			ret = 0;
			break;
		} else if (abort || stp_dbg_get_coredump_timer_state() == CORE_DUMP_TIMEOUT) {
			STP_DBG_ERR_FUNC("paged dump fail, generate fake coredump message\n");
			stp_dbg_set_coredump_timer_state(CORE_DUMP_DOING);
			if (dump_sink == 1)
				stp_dbg_aee_send(FAKECOREDUMPEND, osal_sizeof(FAKECOREDUMPEND), 0);
			else if (dump_sink == 2)
				stp_dbg_nl_send_data(FAKECOREDUMPEND, osal_sizeof(FAKECOREDUMPEND));
			stp_dbg_set_coredump_timer_state(CORE_DUMP_TIMEOUT);
			stp_dbg_poll_cpupcr(5, 5, 0);
			stp_dbg_poll_dmaregs(5, 1);
			ret = -1;
			break;
		}
	} while (1);

	return ret;
}

static _osal_inline_ INT32 stp_dbg_soc_paged_trace(VOID)
{
	INT32 ret = 0;
	UINT32 ctrl_val = 0;
	UINT32 loop_cnt1 = 0;
	UINT32 buffer_start = 0;
	UINT32 buffer_idx = 0;
	PUINT8 dump_vir_addr = NULL;
	P_CONSYS_EMI_ADDR_INFO p_ecsi;
	INT32 dump_len = 0;

	p_ecsi = wmt_plat_get_emi_phy_add();
	do {
		ctrl_val = 0;
		loop_cnt1 = 0;
		buffer_start = 0;
		buffer_idx = 0;
		dump_vir_addr = NULL;

		while (loop_cnt1 < 10) {
			ctrl_val = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_state);
			if (ctrl_val == 0x8)
				break;
			osal_sleep_ms(10);
			loop_cnt1++;
		}
		if (loop_cnt1 >= 10) {
			STP_DBG_ERR_FUNC("polling CTRL STATE fail\n");
			ret = -1;
			break;
		}

		buffer_start = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_print_buff_start);
		buffer_idx = wmt_plat_get_dump_info(p_ecsi->p_ecso->emi_apmem_ctrl_chip_print_buff_idx);
		g_paged_trace_len = buffer_idx;
		STP_DBG_INFO_FUNC("paged trace buffer addr(%08x),buffer_len(%d)\n", buffer_start,
				buffer_idx);
		dump_vir_addr = wmt_plat_get_emi_virt_add(buffer_start - p_ecsi->emi_phy_addr);
		if (!dump_vir_addr) {
			STP_DBG_ERR_FUNC("get vir dump address fail\n");
			ret = -2;
			break;
		}
		osal_memcpy_fromio(&g_paged_trace_buffer[0], dump_vir_addr,
				buffer_idx < STP_DBG_PAGED_TRACE_SIZE ? buffer_idx : STP_DBG_PAGED_TRACE_SIZE);
		/*moving paged trace according to buffer_start & buffer_len */

		dump_len =
			buffer_idx < STP_DBG_PAGED_TRACE_SIZE ? buffer_idx : STP_DBG_PAGED_TRACE_SIZE;
		pr_info("-- paged trace ascii output --");
		stp_dbg_dump_log(&g_paged_trace_buffer[0], dump_len);
		ret = 0;
	} while (0);

	return ret;
}

INT32 stp_dbg_soc_core_dump(INT32 dump_sink)
{
	INT32 ret = 0;

	stp_dbg_soc_paged_dump(dump_sink);
	ret = stp_dbg_soc_paged_trace();
	if (ret)
		STP_DBG_ERR_FUNC("stp_dbg_soc_paged_trace fail: %d!\n", ret);

	if (dump_sink == 0 || chip_reset_only == 1) {
		chip_reset_only = 0;
		mtk_wcn_stp_ctx_restore();
	} else if (dump_sink == 1 || dump_sink == 2) {
		if (stp_dbg_start_emi_dump() < 0)
			mtk_wcn_stp_ctx_restore();
	}

	return ret;
}

PUINT8 stp_dbg_soc_id_to_task(UINT32 id)
{
	UINT32 chip_id = wmt_plat_get_soc_chipid();
	UINT32 task_id_indx = SOC_TASK_ID_GEN2;
	INT32 task_id_flag = 0;

	switch (chip_id) {
	case 0x6797:
	case 0x6759:
	case 0x6758:
	case 0x6775:
	case 0x6771:
	case 0x6765:
		task_id_indx = SOC_TASK_ID_GEN3;
		if (id >= SOC_GEN3_TASK_ID_MAX)
			task_id_flag = SOC_GEN3_TASK_ID_MAX;
		break;
	default:
		task_id_indx = SOC_TASK_ID_GEN2;
		if (id >= SOC_GEN2_TASK_ID_MAX)
			task_id_flag = SOC_GEN2_TASK_ID_MAX;
		break;
	}

	if (task_id_flag) {
		STP_DBG_ERR_FUNC("task id(%d) overflow(%d)\n", id, task_id_flag);
		return NULL;
	} else
		return soc_task_str[task_id_indx][id];
}

UINT32 stp_dbg_soc_read_debug_crs(ENUM_CONNSYS_DEBUG_CR cr)
{
#define CONSYS_REG_READ(addr) (*((volatile UINT32 *)(addr)))
#ifdef CONFIG_OF		/*use DT */
	P_CONSYS_EMI_ADDR_INFO emi_phy_addr;
	UINT32 chip_id = 0;

	chip_id = wmt_plat_get_soc_chipid();
	emi_phy_addr = mtk_wcn_consys_soc_get_emi_phy_add();

	if (cr == CONNSYS_EMI_REMAP) {
		if (emi_phy_addr->emi_remap_offset)
			return CONSYS_REG_READ(conn_reg.topckgen_base +
					emi_phy_addr->emi_remap_offset);
		else
			STP_DBG_INFO_FUNC("EMI remap has no value\n");
	}

	if (chip_id == 0x6765)
		return 0;

	if (conn_reg.mcu_base) {
		switch (cr) {
		case CONNSYS_CPU_CLK:
			return CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_CPU_CLK_STATUS_OFFSET);
		case CONNSYS_BUS_CLK:
			return CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_BUS_CLK_STATUS_OFFSET);
		case CONNSYS_DEBUG_CR1:
			return CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_DBG_CR1_OFFSET);
		case CONNSYS_DEBUG_CR2:
			return CONSYS_REG_READ(conn_reg.mcu_base + CONSYS_DBG_CR2_OFFSET);
		default:
			return 0;
		}
	}
#endif
	return -1;
}
