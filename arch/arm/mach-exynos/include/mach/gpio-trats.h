/* linux/arch/arm/mach-exynos/include/mach/gpio-midas.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4 - MIDAS GPIO lib
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_GPIO_TRATS_H
#define __ASM_ARCH_GPIO_TRATS_H __FILE__

/* MACH_MIDAS_01_BD nor MACH_MIDAS_01_BD nomore exists
   but SLP use GPIO_MIDAS_01_BD, GPIO_MIDAS_02_BD */
#if defined(CONFIG_MACH_SLP_PQ)
#include "gpio-trats2.h"
#elif defined(CONFIG_MACH_TRATS)
#include "gpio-trats1.h"
#endif

#endif /* __ASM_ARCH_GPIO_MIDAS_H */
