/* include/linux/partialsuspend_slp.h
 *
 * Copyright (C) 2012 SAMSUNG, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_PARTIALSUSPEND_H
#define _LINUX_PARTIALSUSPEND_H

#include <linux/list.h>

struct pre_suspend {
	struct list_head link;
	int level;
	void (*suspend)(struct pre_suspend *h);
	void (*resume)(struct pre_suspend *h);
};

struct suspend_status_struct {
	char req;
	char ing;
	char done;
};

void register_pre_suspend(struct pre_suspend *handler);
void unregister_pre_suspend(struct pre_suspend *handler);
#endif
