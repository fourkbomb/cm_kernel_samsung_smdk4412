/*
 * Based on Android early suspend
 * kernel/power/partialsuspend_slp.c
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

#include <linux/partialsuspend_slp.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/syscalls.h> /* sys_sync */
#include <linux/workqueue.h>

#include "power.h"

#define WORK_QUEUE						1
#define PARTIALSUSPEND_SLP_DEBUG		1

#define ACTION		1
#define INACTION	0

static void sync_system(struct work_struct *work);
#if WORK_QUEUE
static void pre_suspend(struct work_struct *work);
static void post_resume(struct work_struct *work);
#else
static void pre_suspend(void);
static void post_resume(void);
#endif
/* static void suspend(struct work_struct *work); */
static void suspend(void);

static DEFINE_MUTEX(pre_suspend_lock);
static LIST_HEAD(pre_suspend_handlers);
static DEFINE_SPINLOCK(state_lock);
static DECLARE_WORK(sync_system_work, sync_system);

#if WORK_QUEUE
static DECLARE_WORK(pre_suspend_work, pre_suspend);
static DECLARE_WORK(post_resume_work, post_resume);
#endif
/* static DECLARE_WORK(suspend_work, suspend); */

#ifdef PARTIALSUSPEND_SLP_DEBUG
#define partialsusp_debug(fmt, arg...) \
	printk(KERN_CRIT "-----" fmt "\n", ## arg)
#endif

static struct suspend_status_struct pre, post, sus;

struct workqueue_struct *sync_work_queue;
struct workqueue_struct *suspend_work_queue;
suspend_state_t requested_suspend_state = PM_SUSPEND_MEM;

static void sync_system(struct work_struct *work)
{
	sys_sync();
}

/* static void suspend(struct work_struct *work) */
static void suspend(void)
{
	unsigned long irqflags;

	spin_lock_irqsave(&state_lock, irqflags);
	if (sus.req == ACTION) {
		sus.ing = INACTION;
		sus.done = ACTION;
		pre.done = INACTION;
		post.done = INACTION;
	}
	spin_unlock_irqrestore(&state_lock, irqflags);

	enter_state(requested_suspend_state);
}

void register_pre_suspend(struct pre_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&pre_suspend_lock);
	list_for_each(pos, &pre_suspend_handlers) {
		struct pre_suspend *e;
		e = list_entry(pos, struct pre_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&pre_suspend_lock);
}
EXPORT_SYMBOL(register_pre_suspend);

void unregister_pre_suspend(struct pre_suspend *handler)
{
	mutex_lock(&pre_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&pre_suspend_lock);
}
EXPORT_SYMBOL(unregister_pre_suspend);

#if WORK_QUEUE
static void pre_suspend(struct work_struct *work)
#else
static void pre_suspend(void)
#endif
{
	struct pre_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

	partialsusp_debug("[SLP-PartialSuspend] %s >> %s\n",
		__FILE__, __func__);

	mutex_lock(&pre_suspend_lock);
	/* spin_lock_irqsave(&state_lock, irqflags); */
	if (pre.req == ACTION) {
		pre.req = INACTION;
		pre.ing = ACTION;
	} else
		abort = 1;
	/* spin_unlock_irqrestore(&state_lock, irqflags); */

	if (abort) {
		partialsusp_debug("[SLP-PartialSuspend] pre_suspend: abort, pre.req %d\n",
			pre.req);
		mutex_unlock(&pre_suspend_lock);
		return;
	}

	partialsusp_debug("[SLP-PartialSuspend] pre_suspend: call handlers\n");

	list_for_each_entry(pos, &pre_suspend_handlers, link) {
		if (pos->suspend != NULL) {
			partialsusp_debug("[SLP-PartialSuspend] pre_suspend: calling %pf\n",
				pos->suspend);
			pos->suspend(pos);
		}
	}
	mutex_unlock(&pre_suspend_lock);

	partialsusp_debug("[SLP-PartialSuspend] pre_suspend: done\n");

#if 0
	partialsusp_debug("[SLP-PartialSuspend] pre_suspend: sync\n");

	queue_work(sync_work_queue, &sync_system_work);
#endif

	spin_lock_irqsave(&state_lock, irqflags);
	if (pre.ing == ACTION) {
		pre.ing = INACTION;
		pre.done = ACTION;
		post.done = INACTION;
		sus.done = INACTION;
	}
	spin_unlock_irqrestore(&state_lock, irqflags);
}

#if WORK_QUEUE
static void post_resume(struct work_struct *work)
#else
static void post_resume(void)
#endif
{
	struct pre_suspend *pos;
	unsigned long irqflags;
	int abort = 0;

	partialsusp_debug("[SLP-PartialSuspend] %s >> %s\n",
		__FILE__, __func__);

	mutex_lock(&pre_suspend_lock);
	/* spin_lock_irqsave(&state_lock, irqflags); */
	if (post.req == ACTION) {
		post.req = INACTION;
		post.ing = ACTION;
	} else
		abort = 1;
	/* spin_unlock_irqrestore(&state_lock, irqflags); */

	if (abort) {
		partialsusp_debug("[SLP-PartialSuspend] post_resume: abort, post.req %d\n",
			post.req);
		mutex_unlock(&pre_suspend_lock);
		return;
	}

	partialsusp_debug("[SLP-PartialSuspend] post_resume: call handlers\n");
	list_for_each_entry_reverse(pos, &pre_suspend_handlers, link) {
		if (pos->resume != NULL) {
			partialsusp_debug("[SLP-PartialSuspend] post_resume: calling %pf\n",
				pos->resume);

			pos->resume(pos);
		}
	}
	mutex_unlock(&pre_suspend_lock);

	partialsusp_debug("[SLP-PartialSuspend] post_resume: done\n");

	spin_lock_irqsave(&state_lock, irqflags);
	if (post.ing == ACTION) {
		post.ing = INACTION;
		post.done = ACTION;
		pre.done = INACTION;
		sus.done = INACTION;
	}
	spin_unlock_irqrestore(&state_lock, irqflags);
}

void request_slp_suspend_state(suspend_state_t new_state)
{
	unsigned long irqflags;

	printk(KERN_ERR "new_state : %d\n", new_state);

	printk(KERN_ERR "pre.ing, pre.done : %d, %d\n", pre.ing, pre.done);
	printk(KERN_ERR "post.ing, post.done : %d, %d\n", post.ing, post.done);
	printk(KERN_ERR "sus.ing, sus.done : %d, %d\n", sus.ing, sus.done);

	if ((new_state == PM_SUSPEND_PRE) &&
			(pre.ing == INACTION) && (pre.done == INACTION)) {

		spin_lock_irqsave(&state_lock, irqflags);
		pre.req = ACTION;
		spin_unlock_irqrestore(&state_lock, irqflags);

#if WORK_QUEUE
		queue_work(suspend_work_queue, &pre_suspend_work);
#else
		pre_suspend();
#endif
	} else if ((new_state == PM_SUSPEND_ON) &&
			(post.ing == INACTION) && (post.done == INACTION)) {

		spin_lock_irqsave(&state_lock, irqflags);
		post.req = ACTION;
		spin_unlock_irqrestore(&state_lock, irqflags);

#if WORK_QUEUE
		queue_work(suspend_work_queue, &post_resume_work);
#else
		post_resume();
#endif
	} else if ((new_state == PM_SUSPEND_MEM) &&
			(sus.ing == INACTION)) {

		spin_lock_irqsave(&state_lock, irqflags);
		sus.req = ACTION;
		spin_unlock_irqrestore(&state_lock, irqflags);

		/* queue_work(suspend_work_queue, &suspend_work); */
		suspend();
	}
}

static int __init slp_presuspend_init(void)
{
	suspend_work_queue = alloc_workqueue("suspend", WQ_HIGHPRI, 0);
	if (suspend_work_queue == NULL)
		return -ENOMEM;

	sync_work_queue = create_singlethread_workqueue("sync_system_work");
	if (sync_work_queue == NULL) {
		destroy_workqueue(suspend_work_queue);
		return -ENOMEM;
	}
	return 0;
}

static void  __exit slp_presuspend_exit(void)
{
	destroy_workqueue(suspend_work_queue);
	destroy_workqueue(sync_work_queue);
}

MODULE_DESCRIPTION("'Partial Suspend support for SLP'");
MODULE_LICENSE("GPL");

core_initcall(slp_presuspend_init);
module_exit(slp_presuspend_exit);
