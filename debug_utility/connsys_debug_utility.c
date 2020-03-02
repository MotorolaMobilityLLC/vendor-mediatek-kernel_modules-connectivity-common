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

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include "connsys_debug_utility.h"
#include "ring.h"
#ifdef EMI_TO_CACHE_SUPPORT
#include "ring_cache.h"
#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
static phys_addr_t gPhyAddrEmiBase;
static void __iomem *gVirAddrEmiLogBase;
static int gConn2ApIrqId;
static void *gIrqRegBaseVirAddr;
#ifdef PRINT_FW_LOG
static void *gLog_data;
#endif
static struct work_struct gLogDataWorker;

static CONNLOG_EVENT_CB event_callback_table[CONNLOG_TYPE_END] = { 0x0 };

struct connlog_buffer {
	struct ring ring_emi;
#ifdef EMI_TO_CACHE_SUPPORT
	struct ring_cache ring_cache;
	void *cache_base;
	unsigned cache_read;
	unsigned cache_write;
#endif
};
static struct connlog_buffer connlog_buffer_table[CONNLOG_TYPE_END];

struct connlog_offset {
	unsigned int emi_base_offset;
	unsigned int emi_size;
	unsigned int emi_read;
	unsigned int emi_write;
	unsigned int emi_buf;
};

#define INIT_EMI_OFFSET(base, size, read, write, buf) {\
	.emi_base_offset = base, \
	.emi_size = size, \
	.emi_read = read, \
	.emi_write = write, \
	.emi_buf = buf}
static struct connlog_offset emi_offset_table[CONNLOG_TYPE_END] = {
	INIT_EMI_OFFSET(CONNLOG_EMI_WIFI_BASE_OFFESET, CONNLOG_EMI_WIFI_SIZE,
		CONNLOG_EMI_WIFI_READ, CONNLOG_EMI_WIFI_WRITE, CONNLOG_EMI_WIFI_BUF),
	INIT_EMI_OFFSET(CONNLOG_EMI_BT_BASE_OFFESET, CONNLOG_EMI_BT_SIZE,
		CONNLOG_EMI_BT_READ, CONNLOG_EMI_BT_WRITE, CONNLOG_EMI_BT_BUF),
	INIT_EMI_OFFSET(CONNLOG_EMI_GPS_BASE_OFFESET, CONNLOG_EMI_GPS_SIZE,
		CONNLOG_EMI_GPS_READ, CONNLOG_EMI_GPS_WRITE, CONNLOG_EMI_GPS_BUF),
	INIT_EMI_OFFSET(CONNLOG_EMI_MCU_BASE_OFFESET, CONNLOG_EMI_MCU_SIZE,
		CONNLOG_EMI_MCU_READ, CONNLOG_EMI_MCU_WRITE, CONNLOG_EMI_MCU_BUF),
};


static char *type_to_title[CONNLOG_TYPE_END] = {
	"wifi_fw", "bt_fw", "gps_fw", "mcu_fw"
};

#ifdef EMI_TO_CACHE_SUPPORT
static size_t cache_size_table[CONNLOG_TYPE_END] = {
	CONNLOG_EMI_WIFI_SIZE * 2, CONNLOG_EMI_BT_SIZE * 2,
	CONNLOG_EMI_GPS_SIZE, CONNLOG_EMI_MCU_SIZE
};
#endif
/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static int connlog_eirq_init(void *irq_reg_base, unsigned int irq_id, unsigned int irq_flag);
static void connlog_eirq_deinit(void);
static int connlog_emi_init(phys_addr_t emiaddr);
static void connlog_emi_deinit(void);
static int connlog_ring_buffer_init(void);
static void connlog_ring_buffer_deinit(void);
static int connlog_set_ring_buffer_base_addr(void);
#ifdef CONN_LOG_TEST
static void connlog_softirq_isr(struct softirq_action *act);
#else
static void connlog_clear_irq_reg(void);
static irqreturn_t connlog_eirq_isr(int irq, void *arg);
#endif
static void connlog_set_ring_ready(void);
static void connlog_buffer_init(int conn_type);
#ifdef EMI_TO_CACHE_SUPPORT
static void connlog_ring_emi_to_cache(int conn_type);
#endif
static void connlog_dump_buf(const char *title, const char *buf, ssize_t sz);
#ifdef PRINT_FW_LOG
static void connlog_ring_print(int conn_type);
#endif
static void connlog_event_set(int conn_type);
static void connlog_log_data_handler(struct work_struct *work);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
static void connlog_set_ring_ready(void);

/*****************************************************************************
* FUNCTION
*  connlog_cache_allocate
* DESCRIPTION
*  Allocate memroy for cache .
* PARAMETERS
*  size      [IN]        data buffer length
* RETURNS
*  void*  buffer pointer
*****************************************************************************/
void *connlog_cache_allocate(size_t size)
{
	void *pBuffer = NULL;

	pBuffer = kmalloc(size, GFP_KERNEL);
	if (!pBuffer)
		return NULL;
	return pBuffer;
}

/*  */
/*****************************************************************************
* FUNCTION
*  connlog_set_ring_ready
* DESCRIPTION
*  set reserved bit [63:56] be MTK36500 to indicate that init is ready.
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static void connlog_set_ring_ready(void)
{
	const char ready_str[] = "MTK36500";

	memcpy_toio(gVirAddrEmiLogBase + CONNLOG_READY_PATTERN_BASE,
		ready_str, CONNLOG_READY_PATTERN_BASE_SIZE);
}

/*****************************************************************************
* FUNCTION
*  connlog_buffer_init
* DESCRIPTION
*  Initialize ring and cache buffer
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  void
*****************************************************************************/
static void connlog_buffer_init(int conn_type)
{
	/* init ring emi */
	ring_init(
		gVirAddrEmiLogBase + emi_offset_table[conn_type].emi_buf,
		emi_offset_table[conn_type].emi_size,
		gVirAddrEmiLogBase + emi_offset_table[conn_type].emi_read,
		gVirAddrEmiLogBase + emi_offset_table[conn_type].emi_write,
		&connlog_buffer_table[conn_type].ring_emi
	);

#ifdef EMI_TO_CACHE_SUPPORT
	/* init ring cache */
	connlog_buffer_table[conn_type].cache_base = connlog_cache_allocate(cache_size_table[conn_type]);
	memset(connlog_buffer_table[conn_type].cache_base, 0, cache_size_table[conn_type]);
	connlog_buffer_table[conn_type].cache_read = 0;
	connlog_buffer_table[conn_type].cache_write = 0;
	ring_cache_init(
		connlog_buffer_table[conn_type].cache_base,
		cache_size_table[conn_type],
		&connlog_buffer_table[conn_type].cache_read,
		&connlog_buffer_table[conn_type].cache_write,
		&connlog_buffer_table[conn_type].ring_cache
	);
#endif
}

#ifdef EMI_TO_CACHE_SUPPORT
/*****************************************************************************
* FUNCTION
*  connlog_ring_emi_to_cache
* DESCRIPTION
*  copy data from emi ring buffer to cache
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  int    0=failed, others=buffer length
*****************************************************************************/
static void connlog_ring_emi_to_cache(int conn_type)
{
	struct ring_segment ring_seg;
	struct ring *ring = &connlog_buffer_table[conn_type].ring_emi;
	struct ring_cache *ring_cache = &connlog_buffer_table[conn_type].ring_cache;
	int total_size = 0;
	int count = 0;
	unsigned int cache_max_size = 0;

	if (RING_CACHE_FULL(ring_cache)) {
		pr_warn("%s cache is full.\n", type_to_title[conn_type]);
		return;
	}

	cache_max_size = RING_CACHE_WRITE_REMAIN_SIZE(ring_cache);
	if (RING_EMPTY(ring) || !ring_read_prepare(cache_max_size, &ring_seg, ring)) {
		pr_err("%s no data, possibly taken by concurrent reader.\n", type_to_title[conn_type]);
		return;
	}

	/* Check ring buffer memory. Dump EMI data if it's corruption. */
	if (EMI_READ32(ring->read) > emi_offset_table[conn_type].emi_size ||
		EMI_READ32(ring->write) > emi_offset_table[conn_type].emi_size) {
		pr_err("%s read/write pointer out-of-bounds.\n", type_to_title[conn_type]);
		/* 64 byte ring buffer setting & 32 byte mcu read/write pointer */
		connlog_dump_emi(0x0, 0x60);
		/* 32 byte wifi read/write pointer */
		connlog_dump_emi(CONNLOG_EMI_WIFI_BASE_OFFESET, 0x20);
		/* 32 byte bt read/write pointer */
		connlog_dump_emi(CONNLOG_EMI_BT_BASE_OFFESET, 0x20);
		/* 32 byte gps read/write pointer */
		connlog_dump_emi(CONNLOG_EMI_GPS_BASE_OFFESET, 0x20);
	}

	RING_READ_ALL_FOR_EACH(ring_seg, ring) {
		struct ring_cache_segment ring_cache_seg;
		unsigned int emi_buf_size = ring_seg.sz;
		unsigned int written = 0;

		ring_dump(__func__, ring);
		ring_dump_segment(__func__, &ring_seg);
		pr_warn("%s: count(%d), ring_seg.sz(%d)\n", type_to_title[conn_type], count, ring_seg.sz);

		ring_cache_write_prepare(ring_seg.sz, &ring_cache_seg, &connlog_buffer_table[conn_type].ring_cache);
		RING_CACHE_WRITE_FOR_EACH(emi_buf_size, ring_cache_seg, &connlog_buffer_table[conn_type].ring_cache) {
			ring_cache_dump(__func__, &connlog_buffer_table[conn_type].ring_cache);
			ring_cache_dump_segment(__func__, &ring_cache_seg);

			pr_info("ring_pt=%p, ring_seg.sz=%d", ring_cache_seg.ring_cache_pt, ring_cache_seg.sz);
			memcpy_fromio(ring_cache_seg.ring_cache_pt, ring_seg.ring_pt + ring_cache_seg.data_pos,
				ring_cache_seg.sz);
			emi_buf_size -= ring_cache_seg.sz;
			written += ring_cache_seg.sz;
		}

		total_size += ring_seg.sz;
		count++;
	}
}
#endif
/* output format
 * xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx ................
 * 3 digits hex * 16 + 16 single char + 1 NULL terminate = 64+1 bytes
 */
#define BYETES_PER_LINE 16
#define LOG_LINE_SIZE (3*BYETES_PER_LINE + BYETES_PER_LINE + 1)
#define IS_VISIBLE_CHAR(c) ((c) >= 32 && (c) <= 126)
static void connlog_dump_buf(const char *title, const char *buf, ssize_t sz)
{
	int i;
	char line[LOG_LINE_SIZE];

	i = 0;
	line[LOG_LINE_SIZE-1] = 0;
	while (sz--) {
		snprintf(line + i*3, 3, "%02x", *buf);
		line[i*3 + 2] = ' ';

		if (IS_VISIBLE_CHAR(*buf))
			line[3*BYETES_PER_LINE + i] = *buf;
		else
			line[3*BYETES_PER_LINE + i] = '`';

		i++;
		buf++;

		if (i >= BYETES_PER_LINE || !sz) {
			if (i < BYETES_PER_LINE) {
				memset(line+i*3, ' ', (BYETES_PER_LINE-i)*3);
				memset(line+3*BYETES_PER_LINE+i, '.', BYETES_PER_LINE-i);
			}
			pr_info("%s: %s\n", title, line);
			i = 0;
		}
	}
}

#ifdef PRINT_FW_LOG
#define LOG_MAX_LEN 1024
#define LOG_HEAD_LENG 16
#define TIMESYNC_LENG 40
const char log_head[] = {0x62, 0x00, 0x00, 0x00};
const char timesync_head[] = {0x55, 0x00, 0x25, 0x62};
static char log_line[LOG_MAX_LEN];
static void connlog_fw_log_parser(int conn_type, const char *buf, ssize_t sz)
{
	unsigned int utc_s = 0;
	unsigned int utc_us = 0;
	unsigned int buf_len = 0;
	unsigned int print_len = 0;

	while (sz > LOG_HEAD_LENG) {
		if (*buf == log_head[0]) {
			if (!memcmp(buf, log_head, sizeof(log_head))) {
				buf_len = buf[14] + (buf[15] << 8);
				print_len = buf_len >= LOG_MAX_LEN ? LOG_MAX_LEN - 1 : buf_len;
				memcpy(log_line, buf + LOG_HEAD_LENG, print_len);
				log_line[print_len] = 0;
				pr_info("%s: %s\n", type_to_title[conn_type], log_line);
				sz -= (LOG_HEAD_LENG + buf_len);
				buf += (LOG_HEAD_LENG + buf_len);
				continue;
			}
		} else if (*buf == timesync_head[0] && sz >= TIMESYNC_LENG) {
			if (!memcmp(buf, timesync_head, sizeof(timesync_head))) {
				memcpy(&utc_s, buf + 32, sizeof(utc_s));
				memcpy(&utc_us, buf + 36, sizeof(utc_us));
				pr_info("%s: timesync :  %u.%u\n", type_to_title[conn_type], utc_s, utc_us);
				sz -= TIMESYNC_LENG;
				buf += TIMESYNC_LENG;
				continue;
			}
		}
		sz--;
		buf++;
	}
}
/*****************************************************************************
* FUNCTION
*  connlog_ring_print
* DESCRIPTION
*  print log data on kernel log
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  void
*****************************************************************************/
static void connlog_ring_print(int conn_type)
{
	unsigned int written = 0;
	unsigned int buf_size;
	struct ring_segment ring_seg;
	struct ring *ring;

	ring = &connlog_buffer_table[conn_type].ring_emi;
	if (RING_EMPTY(ring) || !ring_read_all_prepare(&ring_seg, ring)) {
		pr_err("type(%s) no data, possibly taken by concurrent reader.\n", type_to_title[conn_type]);
		return;
	}
	buf_size = ring_seg.remain;
	memset(gLog_data, 0, CONNLOG_EMI_BT_SIZE);

	/* for debugging, must be removed before SQC { */
	if (conn_type == CONNLOG_TYPE_MCU) {
		pr_err("It's impossible that MCU ring buffer has data.\n");
		connlog_dump_emi(0x0, 0x200);
	}
	/* for debugging, must be removed before SQC } */
	/* Check ring buffer memory. Dump EMI data if it's corruption. */
	if (EMI_READ32(ring->read) > emi_offset_table[conn_type].emi_size ||
		EMI_READ32(ring->write) > emi_offset_table[conn_type].emi_size) {
		pr_err("%s read/write pointer out-of-bounds.\n", type_to_title[conn_type]);
		/* 64 byte ring buffer setting & 32 byte mcu read/write pointer */
		connlog_dump_emi(0x0, 0x60);
		/* 32 byte wifi read/write pointer */
		connlog_dump_emi(CONNLOG_EMI_WIFI_BASE_OFFESET, 0x20);
		/* 32 byte bt read/write pointer */
		connlog_dump_emi(CONNLOG_EMI_BT_BASE_OFFESET, 0x20);
		/* 32 byte gps read/write pointer */
		connlog_dump_emi(CONNLOG_EMI_GPS_BASE_OFFESET, 0x20);
	}

	RING_READ_ALL_FOR_EACH(ring_seg, ring) {
		memcpy_fromio(gLog_data + written, ring_seg.ring_pt, ring_seg.sz);
		/* connlog_dump_buf("fw_log", gLog_data + written, ring_seg.sz); */
		buf_size -= ring_seg.sz;
		written += ring_seg.sz;
	}
	if (conn_type != CONNLOG_TYPE_BT)
		connlog_fw_log_parser(conn_type, gLog_data, written);
}
#endif
/*****************************************************************************
* FUNCTION
*  connlog_event_set
* DESCRIPTION
*  Trigger  event call back to wakeup waitqueue
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  void
*****************************************************************************/
static void connlog_event_set(int conn_type)
{
	if ((conn_type < CONNLOG_TYPE_END) && (event_callback_table[conn_type] != 0x0))
		(*event_callback_table[conn_type])();
}

/*****************************************************************************
* FUNCTION
*  connlog_print_log
* DESCRIPTION
*  Print FW log to kernel log
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  void
*****************************************************************************/
static void connlog_log_data_handler(struct work_struct *work)
{
	int ret = 0;
	int i;

	do {
		ret = 0;
		for (i = 0; i < CONNLOG_TYPE_END; i++) {
			if (!RING_EMPTY(&connlog_buffer_table[i].ring_emi)) {
#ifdef EMI_TO_CACHE_SUPPORT
				connlog_ring_emi_to_cache(i);
#endif
#ifdef PRINT_FW_LOG
				connlog_ring_print(i);
#endif
				connlog_event_set(i);
				/* ret++; */
			} else
				pr_info("%s emi ring is empty!!\n", type_to_title[i]);
		}
	} while (ret);
#ifndef CONN_LOG_TEST
	connlog_clear_irq_reg();
	enable_irq(gConn2ApIrqId);
#endif
}

#ifdef CONN_LOG_TEST
/*****************************************************************************
* FUNCTION
*  connlog_softirq_isr
* DESCRIPTION
*  To simulate IRQ behavior
* PARAMETERS
*  act      [IN]        irq action
* RETURNS
*  void
*****************************************************************************/
static void connlog_softirq_isr(struct softirq_action *act)
{
	schedule_work(&gLogDataWorker);
}
#else
/*****************************************************************************
* FUNCTION
*  connlog_clear_irq_reg
* DESCRIPTION
*  Clear irq bits in conn2ap_sw_irq
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static void connlog_clear_irq_reg(void)
{
	if (gIrqRegBaseVirAddr) {
		pr_info("BF: CONSYS IRQ CR VALUE(0x%x)\n", EMI_READ32(gIrqRegBaseVirAddr));
		EMI_WRITE32(gIrqRegBaseVirAddr, EMI_READ32(gIrqRegBaseVirAddr) & (!0xF000000)); /* 18002150[27:24] */
		pr_info("AF: CONSYS IRQ CR VALUE(0x%x)\n", EMI_READ32(gIrqRegBaseVirAddr));
	} else
		pr_err("MCU register ioremap fail!\n");
}

/*****************************************************************************
* FUNCTION
*  connlog_eirq_isr
* DESCRIPTION
*  IRQ handler to notify subsys that EMI has logs.
* PARAMETERS
*  irq      [IN]        irq number
*  art      [IN]        other argument
* RETURNS
*  irqreturn_t           irq status
*     @IRQ_HANDLED       interrupt was handled by this device
*****************************************************************************/
static irqreturn_t connlog_eirq_isr(int irq, void *arg)
{
	schedule_work(&gLogDataWorker);
	disable_irq_nosync(gConn2ApIrqId);
	return IRQ_HANDLED;
}
#endif

/*****************************************************************************
* FUNCTION
*  connlog_eirq_init
* DESCRIPTION
*  To register IRQ
* PARAMETERS
*  irq_id      [IN]        irq number
*  irq_reg_base[IN]        irq register base address
* RETURNS
*  int    0=success, others=error
*****************************************************************************/
static int connlog_eirq_init(void *irq_reg_base, unsigned int irq_id, unsigned int irq_flag)
{
	int iret = 0;

	if (gConn2ApIrqId == 0) {
		gConn2ApIrqId = irq_id;
		gIrqRegBaseVirAddr = irq_reg_base;
	} else {
		pr_warn("IRQ has been initialized\n");
		return -1;
	}

#ifdef CONN_LOG_TEST
	/* need to add CONN_LOG_SOFTIRQ in include/linux/interrupt.h */
	open_softirq(gConn2ApIrqId, connlog_softirq_isr);
#else
	iret = request_irq(gConn2ApIrqId, connlog_eirq_isr, irq_flag, "CONN_LOG_IRQ", NULL);
	if (iret)
		pr_err("EINT IRQ(%d) NOT AVAILABLE!!\n", gConn2ApIrqId);
#endif
	return iret;
}

/*****************************************************************************
* FUNCTION
*  connlog_eirq_deinit
* DESCRIPTION
*  unrigester irq
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static void connlog_eirq_deinit(void)
{
#ifndef CONN_LOG_TEST
	free_irq(gConn2ApIrqId, NULL);
#endif
}

/*****************************************************************************
* FUNCTION
*  connlog_set_ring_buffer_base_addr
* DESCRIPTION
*  Set subsys log base address on EMI for FW
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static int connlog_set_ring_buffer_base_addr(void)
{
	if (!gVirAddrEmiLogBase)
		return -1;

	/* set up subsys base address */
	EMI_WRITE32(gVirAddrEmiLogBase + 0,  CONNLOG_EMI_MCU_BASE_OFFESET);
	EMI_WRITE32(gVirAddrEmiLogBase + 4,  CONNLOG_EMI_MCU_SIZE);
	EMI_WRITE32(gVirAddrEmiLogBase + 8,  CONNLOG_EMI_WIFI_BASE_OFFESET);
	EMI_WRITE32(gVirAddrEmiLogBase + 12, CONNLOG_EMI_WIFI_SIZE);
	EMI_WRITE32(gVirAddrEmiLogBase + 16, CONNLOG_EMI_BT_BASE_OFFESET);
	EMI_WRITE32(gVirAddrEmiLogBase + 20, CONNLOG_EMI_BT_SIZE);
	EMI_WRITE32(gVirAddrEmiLogBase + 24, CONNLOG_EMI_GPS_BASE_OFFESET);
	EMI_WRITE32(gVirAddrEmiLogBase + 28, CONNLOG_EMI_GPS_SIZE);
	return 0;
}

/*****************************************************************************
* FUNCTION
*  connlog_emi_init
* DESCRIPTION
*  Do ioremap for log buffer on EMI
* PARAMETERS
*  emiaddr      [IN]        physical EMI base address
* RETURNS
*  void
*****************************************************************************/
static int connlog_emi_init(phys_addr_t emiaddr)
{
	if (emiaddr == 0) {
		pr_err("consys emi memory address gPhyAddrEmiBase invalid\n");
		return -1;
	}

	if (gPhyAddrEmiBase) {
		pr_warn("emi base address has been initialized\n");
		return -2;
	}

	gPhyAddrEmiBase = emiaddr;
	gVirAddrEmiLogBase = ioremap_nocache(gPhyAddrEmiBase +
		CONNLOG_EMI_LOG_BASE_OFFSET, CONNLOG_EMI_SIZE);
	if (gVirAddrEmiLogBase) {
		pr_info("EMI mapping OK virtual(0x%p) physical(0x%x)\n",
				gVirAddrEmiLogBase, (unsigned int) gPhyAddrEmiBase +
				CONNLOG_EMI_LOG_BASE_OFFSET);
		memset_io(gVirAddrEmiLogBase, 0, CONNLOG_EMI_SIZE);
	} else
		pr_err("EMI mapping fail\n");

	return 0;
}

/*****************************************************************************
* FUNCTION
*  connlog_emi_init
* DESCRIPTION
*  Do iounmap for log buffer on EMI
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static void connlog_emi_deinit(void)
{
	iounmap(gVirAddrEmiLogBase);
}

/*****************************************************************************
* FUNCTION
*  connlog_ring_buffer_init
* DESCRIPTION
*  Initialize ring buffer setting for subsys
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static int connlog_ring_buffer_init(void)
{
	if (!gVirAddrEmiLogBase) {
		pr_err("consys emi memory address gPhyAddrEmiBase invalid\n");
		return -1;
	}

	connlog_set_ring_buffer_base_addr();
	connlog_buffer_init(CONNLOG_TYPE_WIFI);
	connlog_buffer_init(CONNLOG_TYPE_BT);
	connlog_buffer_init(CONNLOG_TYPE_GPS);
	connlog_buffer_init(CONNLOG_TYPE_MCU);
#ifdef PRINT_FW_LOG
	gLog_data = connlog_cache_allocate(CONNLOG_EMI_BT_SIZE);
#endif
	connlog_set_ring_ready();

	return 0;
}

/*****************************************************************************
* FUNCTION
*  connlog_ring_buffer_deinit
* DESCRIPTION
*  Initialize ring buffer setting for subsys
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
static void connlog_ring_buffer_deinit(void)
{
#ifdef EMI_TO_CACHE_SUPPORT
	int i;

	for (i = 0; i < CONNLOG_TYPE_END; i++) {
		kfree(connlog_buffer_table[i].cache_base);
		connlog_buffer_table[i].cache_base = NULL;
	}
#endif
#ifdef PRINT_FW_LOG
	kfree(gLog_data);
	gLog_data = NULL;
#endif
}

/*****************************************************************************
* FUNCTION
*  connsys_dedicated_log_path_apsoc_init
* DESCRIPTION
*  Initialize API for common driver to initialize connsys dedicated log
*  for APSOC platform
* PARAMETERS
*  emiaddr      [IN]        EMI physical base address
*  irq_reg_base [IN]        conn2ap_sw_irq CR base address after ioremapping
*  irq_num      [IN]        IRQ id from device tree
*  irq_flag     [IN]        IRQ flag from device tree
* RETURNS
*  void
*****************************************************************************/
int connsys_dedicated_log_path_apsoc_init(phys_addr_t emiaddr, void *irq_reg_base,
	unsigned int irq_num, unsigned int irq_flag)
{
	if (connlog_emi_init(emiaddr)) {
		pr_err("EMI init failed\n");
		return -1;
	}

	if (connlog_ring_buffer_init()) {
		pr_err("Ring buffer init failed\n");
		return -2;
	}

	if (connlog_eirq_init(irq_reg_base, irq_num, irq_flag)) {
		pr_err("EIRQ init failed\n");
		return -3;
	}

	INIT_WORK(&gLogDataWorker, connlog_log_data_handler);
	return 0;
}
EXPORT_SYMBOL(connsys_dedicated_log_path_apsoc_init);

/*****************************************************************************
* FUNCTION
*  connsys_dedicated_log_path_apsoc_deinit
* DESCRIPTION
*  De-Initialize API for common driver to release cache, un-remap emi and free
*  irq for APSOC platform
* PARAMETERS
*  void
* RETURNS
*  void
*****************************************************************************/
void connsys_dedicated_log_path_apsoc_deinit(void)
{
	connlog_emi_deinit();
	connlog_eirq_deinit();
	connlog_ring_buffer_deinit();
}
EXPORT_SYMBOL(connsys_dedicated_log_path_apsoc_deinit);

/*****************************************************************************
* FUNCTION
*  connsys_log_init
* DESCRIPTION
*  Init API for subsys driver.
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  int    0=success, others=error
*****************************************************************************/
int connsys_log_init(int conn_type)
{
	return 0;
}
EXPORT_SYMBOL(connsys_log_init);

/*****************************************************************************
* FUNCTION
*  connsys_log_deinit
* DESCRIPTION
*  De-init API for subsys driver.
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  int    0=success, others=error
*****************************************************************************/
int connsys_log_deinit(int conn_type)
{
	if (conn_type >= CONNLOG_TYPE_END || conn_type < 0)
		return -1;
	event_callback_table[conn_type] = 0x0;
	return 0;
}
EXPORT_SYMBOL(connsys_log_deinit);

/*****************************************************************************
* FUNCTION
*  connsys_log_get_buf_size
* DESCRIPTION
*  Get ring buffer unread size on EMI.
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  unsigned int    Ring buffer unread size
*****************************************************************************/
unsigned int connsys_log_get_buf_size(int conn_type)
{
	if (conn_type >= CONNLOG_TYPE_END || conn_type < 0)
		return -1;
#ifdef EMI_TO_CACHE_SUPPORT
	return RING_CACHE_SIZE(&connlog_buffer_table[conn_type].ring_cache);
#else
	return RING_SIZE(&connlog_buffer_table[conn_type].ring_emi);
#endif
}
EXPORT_SYMBOL(connsys_log_get_buf_size);

/*****************************************************************************
* FUNCTION
*  connsys_log_register_event_cb
* DESCRIPTION
*  Register callback function. It'll be trigger while receive conn2ap IRQ.
* PARAMETERS
*  conn_type      [IN]        subsys type
*  func           [IN]        callback function pointer
* RETURNS
*  int    0=success, others=error
*****************************************************************************/
int connsys_log_register_event_cb(int conn_type, CONNLOG_EVENT_CB func)
{
	if (conn_type >= CONNLOG_TYPE_END || conn_type < 0)
		return -1;
	event_callback_table[conn_type] = func;
	return 0;
}
EXPORT_SYMBOL(connsys_log_register_event_cb);

static int connlog_copy_buf(int conn_type, int to, char *buf, size_t count)
{
	int retval = 0;
	unsigned int written = 0;
#ifdef EMI_TO_CACHE_SUPPORT
	unsigned int cache_buf_size;
	struct ring_cache_segment ring_seg;
	struct ring_cache *ring = &connlog_buffer_table[conn_type].ring_cache;
	unsigned int size = 0;

	size = count < RING_CACHE_SIZE(ring) ? count : RING_CACHE_SIZE(ring);
	if (RING_CACHE_EMPTY(ring) || !ring_cache_read_prepare(size, &ring_seg, ring)) {
		pr_err("type(%d) no data, possibly taken by concurrent reader.\n", conn_type);
		goto done;
	}
	cache_buf_size = ring_seg.remain;

	RING_CACHE_READ_ALL_FOR_EACH(ring_seg, ring) {
		retval = copy_to_user(buf + written, ring_seg.ring_cache_pt, ring_seg.sz);
		if (retval) {
			pr_err("copy to user buffer failed, ret:%d\n", retval);
			goto done;
		}
		cache_buf_size -= ring_seg.sz;
		written += ring_seg.sz;
	}
#else
	unsigned int buf_size;
	struct ring_segment ring_seg;
	struct ring *ring = &connlog_buffer_table[conn_type].ring_emi;

	if (RING_EMPTY(ring) || !ring_read_all_prepare(&ring_seg, ring)) {
		pr_err("type(%d) no data, possibly taken by concurrent reader.\n", conn_type);
		goto done;
	}
	buf_size = ring_seg.remain;

	RING_READ_ALL_FOR_EACH(ring_seg, ring) {
		switch (to) {
		case 0:
			retval = copy_to_user(buf + written, ring_seg.ring_pt, ring_seg.sz);
			break;
		case 1:
			memcpy(buf + written, ring_seg.ring_pt, ring_seg.sz);
			break;
		default:
			goto done;
		}

		if (retval) {
			pr_err("copy to user buffer failed, ret:%d\n", retval);
			goto done;
		}
		buf_size -= ring_seg.sz;
		written += ring_seg.sz;
	}
#endif
done:
	return written;
}

/*****************************************************************************
* FUNCTION
*  connsys_log_read
* DESCRIPTION
*  Copy EMI ring buffer data to the buffer that provided by sub-module.
* PARAMETERS
*  conn_type      [IN]        subsys type
* RETURNS
*  ssize_t    read buffer size
*****************************************************************************/
ssize_t connsys_log_read(int conn_type, char *buf, size_t count)
{
	return connlog_copy_buf(1, conn_type, buf, count);
}
EXPORT_SYMBOL(connsys_log_read);

/*****************************************************************************
* FUNCTION
*  connsys_log_read_to_user
* DESCRIPTION
*  Copy EMI ring buffer data to the user buffer.
* PARAMETERS
*  conn_type      [IN]        subsys type
*  buf            [IN]        user buffer
*  count          [IN]        buffer length
* RETURNS
*  ssize_t    read buffer size
*****************************************************************************/
ssize_t connsys_log_read_to_user(int conn_type, char __user *buf, size_t count)
{
	int retval;
	unsigned int written = 0;
#ifdef EMI_TO_CACHE_SUPPORT
	unsigned int cache_buf_size;
	struct ring_cache_segment ring_seg;
	struct ring_cache *ring = &connlog_buffer_table[conn_type].ring_cache;
	unsigned int size = 0;

	size = count < RING_CACHE_SIZE(ring) ? count : RING_CACHE_SIZE(ring);
	if (RING_CACHE_EMPTY(ring) || !ring_cache_read_prepare(size, &ring_seg, ring)) {
		pr_err("type(%d) no data, possibly taken by concurrent reader.\n", conn_type);
		goto done;
	}
	cache_buf_size = ring_seg.remain;

	RING_CACHE_READ_ALL_FOR_EACH(ring_seg, ring) {
		retval = copy_to_user(buf + written, ring_seg.ring_cache_pt, ring_seg.sz);
		if (retval) {
			pr_err("copy to user buffer failed, ret:%d\n", retval);
			goto done;
		}
		cache_buf_size -= ring_seg.sz;
		written += ring_seg.sz;
	}
#else
	unsigned int buf_size;
	struct ring_segment ring_seg;
	struct ring *ring = &connlog_buffer_table[conn_type].ring_emi;

	if (RING_EMPTY(ring) || !ring_read_all_prepare(&ring_seg, ring)) {
		pr_err("type(%d) no data, possibly taken by concurrent reader.\n", conn_type);
		goto done;
	}
	buf_size = ring_seg.remain;

	RING_READ_ALL_FOR_EACH(ring_seg, ring) {
		retval = copy_to_user(buf + written, ring_seg.ring_pt, ring_seg.sz);
		if (retval) {
			pr_err("copy to user buffer failed, ret:%d\n", retval);
			goto done;
		}
		buf_size -= ring_seg.sz;
		written += ring_seg.sz;
	}
#endif
done:
	return written;
}
EXPORT_SYMBOL(connsys_log_read_to_user);

/*****************************************************************************
* FUNCTION
*  connsys_log_get_emi_log_base_vir_addr
* DESCRIPTION
*  return EMI log base address whitch has done ioremap.
* PARAMETERS
*  void
* RETURNS
*  void __iomem *    ioremap EMI log base address
*****************************************************************************/
void __iomem *connsys_log_get_emi_log_base_vir_addr(void)
{
	return gVirAddrEmiLogBase;
}
EXPORT_SYMBOL(connsys_log_get_emi_log_base_vir_addr);

void connlog_dump_emi(int offset, int size)
{
	connlog_dump_buf("emi", gVirAddrEmiLogBase + offset, size);
}
