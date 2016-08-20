/*
 * linux/arch/arm/mach-exynos/include/mach/board-melfas.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BOARD_MELFAS_H
#define __BOARD_MELFAS_H __FILE__

#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS
#include <linux/melfas_mms_ts.h>
#endif
#define TSP_POSITION_ROTATED 1
#define TSP_POSITION_DEFAULT 0

void melfas_tsp_init(void);
void melfas_tsp_set_platdata(bool rotate);
void melfas_tsp_set_pdata(bool rotate, bool force);
void tsp_charger_infom(bool en);
#endif /* __BOARD_MELFAS_H */

