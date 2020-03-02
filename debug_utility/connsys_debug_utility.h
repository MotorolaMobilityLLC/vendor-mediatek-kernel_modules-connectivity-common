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

#ifndef _CONNSYS_DEBUG_UTILITY_H_
#define _CONNSYS_DEBUG_UTILITY_H_

#include "connsys_debug_utility_emi.h"
#include <linux/types.h>
#include <linux/compiler.h>


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
enum CONNLOG_TYPE {
	CONNLOG_TYPE_WIFI = 0,
	CONNLOG_TYPE_BT = 1,
	CONNLOG_TYPE_GPS = 2,
	CONNLOG_TYPE_MCU = 3,
	CONNLOG_TYPE_END,
};

typedef void (*CONNLOG_EVENT_CB) (void);

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
void connlog_dump_emi(int offset, int size);
/* Common Driver API */
int connsys_dedicated_log_path_apsoc_init(phys_addr_t emiaddr, unsigned int irq_num, unsigned int irq_flag);
void connsys_dedicated_log_path_apsoc_deinit(void);
void __iomem *connsys_log_get_emi_log_base_vir_addr(void);
void connsys_dedicated_log_get_utc_time(unsigned int *second, unsigned int *usecond);

/* Debug Utility API */
int connsys_log_init(int conn_type);
int connsys_log_deinit(int conn_type);
unsigned int connsys_log_get_buf_size(int conn_type);
int connsys_log_register_event_cb(int conn_type, CONNLOG_EVENT_CB func);
ssize_t connsys_log_read_to_user(int conn_type, char __user *buf, size_t count);
ssize_t connsys_log_read(int conn_type, char *buf, size_t count);

#endif /*_CONNSYS_DEBUG_UTILITY_H_*/
