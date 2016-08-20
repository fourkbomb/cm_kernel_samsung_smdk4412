/*  sound/soc/samsung/slp_jack.c
 *
 *  Copyright (C) 2012 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/jack.h>
#include <sound/jack.h>
#include "slp_jack.h"

#define MAX_ZONE_LIMIT 10
#define MAX_BUTTON_ZONE_LIMIT 6
#define DET_CHECK_TIME_MS 200 /* 200ms */

struct slp_jack_info {
	struct slp_jack_platform_data *pdata;
	struct work_struct detect_work;
	struct delayed_work button_event_work;
	struct work_struct pressed_work;
	struct work_struct released_work;
	struct workqueue_struct *queue;
	struct snd_jack *jack;
	struct device *dev;
	int det_mask;
	int button_mask;
	unsigned int det_irq;
	unsigned int button_irq;
	int pressed;
	int pressed_code;
	unsigned int cur_jack_status;
	unsigned int cur_jack_type;
	void *private_data;
};

/* with some modifications like moving all the gpio structs inside
 * the platform data and getting the name for the switch and
 * gpio_event from the platform data, the driver could support more than
 * one headset jack, but currently user space is looking only for
 * one key file and switch for a headset so it'd be overkill and
 * untestable so we limit to one instantiation for now.
 */
static atomic_t instantiated = ATOMIC_INIT(0);

static void slp_jack_event(struct slp_jack_info *hi, int type)
{
	/* type 1 is jack det
	    type 0 is button */
	if (type == SND_JACK_DET) {
		hi->cur_jack_status &= ~hi->det_mask;
		hi->cur_jack_status |= hi->cur_jack_type & hi->det_mask;
		jack_event_handler("earjack", hi->cur_jack_type);
	} else {
		hi->cur_jack_status &= ~hi->button_mask;
		hi->cur_jack_status |=
			(hi->pressed ?  hi->pressed_code : 0) & hi->button_mask;
		jack_event_handler("earkey", hi->pressed);
	}

	snd_jack_report(hi->jack, hi->cur_jack_status);
}

static void slp_jack_set_type(struct slp_jack_info *hi, int jack_type)
{
	struct slp_jack_platform_data *pdata = hi->pdata;

	/* this can happen during slow inserts where we think we identified
	 * the type but then we get another interrupt and do it again
	 */
	if (jack_type == hi->cur_jack_type) {
		if (jack_type != SND_JACK_HEADSET)
			pdata->set_micbias_state(false);
		return;
	}

	hi->cur_jack_type = jack_type;

	if (jack_type == SND_JACK_HEADSET) {
		slp_jack_event(hi, SND_JACK_DET);
		enable_irq(hi->button_irq);

	} else {

		disable_irq(hi->button_irq);
		slp_jack_event(hi, SND_JACK_DET);
		/* micbias is left enabled for 4pole and disabled otherwise */
		pdata->set_micbias_state(false);
	}
	pr_info("%s : jack_type = %d\n", __func__, jack_type);

}

static void handle_jack_not_inserted(struct slp_jack_info *hi)
{
	slp_jack_set_type(hi, SND_JACK_NONE);
	hi->pdata->set_micbias_state(false);
}

static void determine_jack_type(struct slp_jack_info *hi)
{
	struct slp_jack_zone *zones = hi->pdata->zones;
	int size = hi->pdata->num_zones;
	int count[MAX_ZONE_LIMIT] = {0};
	int adc;
	int i;
	unsigned npolarity = !hi->pdata->det_active_high;

	while (gpio_get_value(hi->pdata->det_gpio) ^ npolarity) {
		adc = hi->pdata->get_adc_value();
		pr_debug("%s: adc = %d\n", __func__, adc);

		/* determine the type of headset based on the
		 * adc value.  An adc value can fall in various
		 * ranges or zones.  Within some ranges, the type
		 * can be returned immediately.  Within others, the
		 * value is considered unstable and we need to sample
		 * a few more types (up to the limit determined by
		 * the range) before we return the type for that range.
		 */
		for (i = 0; i < size; i++) {
			if (adc <= zones[i].adc_high) {
				if (++count[i] > zones[i].check_count) {
					slp_jack_set_type(hi,
							  zones[i].jack_type);
					return;
				}
				msleep(zones[i].delay_ms);
				break;
			}
		}
	}
	/* jack removed before detection complete */
	pr_debug("%s : jack removed before detection complete\n", __func__);
	handle_jack_not_inserted(hi);
}

/* thread run whenever the headset detect state changes (either insertion
 * or removal).
 */
static irqreturn_t slp_jack_detect_irq(int irq, void *dev_id)
{
	struct slp_jack_info *hi = dev_id;

	pm_wakeup_event(hi->dev , DET_CHECK_TIME_MS + 100);

	queue_work(hi->queue, &hi->detect_work);

	return IRQ_HANDLED;
}

void slp_jack_detect_work(struct work_struct *work)
{
	struct slp_jack_info *hi =
		container_of(work, struct slp_jack_info, detect_work);
	struct slp_jack_platform_data *pdata = hi->pdata;
	int time_left_ms = DET_CHECK_TIME_MS;
	unsigned npolarity = !hi->pdata->det_active_high;

	/* set mic bias to enable adc */
	pdata->set_micbias_state(true);

	/* debounce headset jack.  don't try to determine the type of
	 * headset until the detect state is true for a while.
	 */
	while (time_left_ms > 0) {
		if (!(gpio_get_value(hi->pdata->det_gpio) ^ npolarity)) {
			/* jack not detected. */
			handle_jack_not_inserted(hi);
			return;
		}
		msleep(20);
		time_left_ms -= 20;
	}
	/* jack presence was detected the whole time, figure out which type */
	determine_jack_type(hi);
}

static irqreturn_t slp_jack_button_irq(int irq, void *dev_id)
{
	struct slp_jack_info *hi = dev_id;
	unsigned npolarity = hi->pdata->button_active_high;
	int cur_pressed;

	cur_pressed = (gpio_get_value(hi->pdata->button_gpio) ?
		SND_JACK_BTN_PRESSED : SND_JACK_BTN_RELEASED) ^ npolarity;

	if (cur_pressed == SND_JACK_BTN_RELEASED)
		queue_work(hi->queue, &hi->released_work);
	else
		queue_work(hi->queue, &hi->pressed_work);

	return IRQ_HANDLED;
}

void slp_jack_button_released_work(struct work_struct *work)
{
	struct slp_jack_info *hi =
		container_of(work, struct slp_jack_info, released_work);

	if (hi->cur_jack_type != SND_JACK_HEADSET)
		return;

	cancel_delayed_work(&hi->button_event_work);

	/* do nothing */
	if (hi->pressed == SND_JACK_BTN_RELEASED)
		return;

	hi->pressed = SND_JACK_BTN_RELEASED;

	slp_jack_event(hi, SND_JACK_BTN);
}

void slp_jack_button_pressed_work(struct work_struct *work)
{
	struct slp_jack_info *hi =
		container_of(work, struct slp_jack_info, pressed_work);
	struct slp_jack_platform_data *pdata = hi->pdata;
	struct slp_jack_button_zone *btn_zones = pdata->button_zones;
	int adc;
	int i;

	/* do nothing */
	if (hi->pressed == SND_JACK_BTN_PRESSED ||
		hi->cur_jack_type != SND_JACK_HEADSET)
		return;

	msleep(50);

	adc = pdata->get_adc_value();
	pr_info("%s: adc = %d\n", __func__, adc);

	for (i = 0; i < pdata->num_button_zones; i++) {
		if (adc >= btn_zones[i].adc_low &&
			adc <= btn_zones[i].adc_high) {
			hi->pressed_code = SND_JACK_BTN_0 >> i;
			queue_delayed_work(hi->queue, &hi->button_event_work,
				btn_zones[i].debounce_time);
			return;
		}
	}

	pr_warn("%s: key is skipped. ADC value is %d\n", __func__, adc);
}


void slp_jack_button_event_work(struct work_struct *work)
{
	struct slp_jack_info *hi =
	container_of(work, struct slp_jack_info, button_event_work.work);

	/* do nothing */
	if (hi->pressed == SND_JACK_BTN_PRESSED ||
		hi->cur_jack_type != SND_JACK_HEADSET)
		return;

	hi->pressed = SND_JACK_BTN_PRESSED;

	pr_err("%s: keycode=%d, is pressed\n", __func__,
		hi->pressed_code);
	slp_jack_event(hi, SND_JACK_BTN);
}

int slp_jack_init(struct snd_card *card, const char *id, int type,
		struct slp_jack_platform_data *pdata)
{
	struct slp_jack_info *hi;
	/*struct slp_jack_platform_data *pdata = pdev->dev.platform_data;*/
	int ret;
	int i;

	pr_info("%s : Registering jack driver\n", __func__);
	if (!pdata) {
		pr_err("%s : pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	if (!pdata->get_adc_value || !pdata->zones ||
	    !pdata->set_micbias_state || pdata->num_zones > MAX_ZONE_LIMIT ||
	    pdata->num_button_zones > MAX_BUTTON_ZONE_LIMIT) {
		pr_err("%s : need to check pdata\n", __func__);
		return -ENODEV;
	}

	if (atomic_xchg(&instantiated, 1)) {
		pr_err("%s : already instantiated, can only have one\n",
			__func__);
		return -ENODEV;
	}

	hi = kzalloc(sizeof(struct slp_jack_info), GFP_KERNEL);
	if (hi == NULL) {
		pr_err("%s : Failed to allocate memory.\n", __func__);
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	hi->pdata = pdata;
	hi->dev = card->dev;

	snd_jack_new(card, id, type, &hi->jack);
	for (i = 0; i < pdata->num_button_zones; i++) {
		int testbit = SND_JACK_BTN_0 >> i;
		hi->button_mask |= testbit;
		snd_jack_set_key(hi->jack,
			testbit, pdata->button_zones[i].code);
	}

	hi->det_mask = type;
	hi->cur_jack_type = SND_JACK_NONE;
	hi->pressed = SND_JACK_BTN_RELEASED;

	/* gpio & adc function init */
	if (pdata->jack_mach_init)
		pdata->jack_mach_init();

	ret = gpio_request(pdata->det_gpio, "ear_jack_detect");
	if (ret) {
		pr_err("%s : det gpio_request failed for %d\n",
		       __func__, pdata->det_gpio);
		goto err_det_gpio_request;
	}

	ret = gpio_request(pdata->button_gpio, "earkey_send_end");
	if (ret) {
		pr_err("%s : key gpio_request failed for %d\n",
		       __func__, pdata->button_gpio);
		goto err_key_gpio_request;
	}

	INIT_WORK(&hi->detect_work, slp_jack_detect_work);
	INIT_WORK(&hi->pressed_work, slp_jack_button_pressed_work);
	INIT_WORK(&hi->released_work, slp_jack_button_released_work);
	INIT_DELAYED_WORK(&hi->button_event_work, slp_jack_button_event_work);

	hi->queue = create_freezable_workqueue("slp_jack_wq");
	if (hi->queue == NULL) {
		ret = -ENOMEM;
		pr_err("%s: Failed to create workqueue\n", __func__);
		goto err_create_wq_failed;
	}

	hi->det_irq = gpio_to_irq(pdata->det_gpio);
	hi->button_irq = gpio_to_irq(pdata->button_gpio);

#ifdef CONFIG_ARM
	set_irq_flags(hi->button_irq, IRQF_NOAUTOEN | IRQF_VALID);
#endif

	ret = request_irq(hi->button_irq, slp_jack_button_irq,
				IRQF_TRIGGER_RISING |
				IRQF_TRIGGER_FALLING, "slp_jack_button", hi);
	if (ret) {
		pr_err("%s : Failed to request_irq for btn.\n", __func__);
		goto err_request_detect_btn_irq;
	}

	queue_work(hi->queue, &hi->detect_work);

	ret = request_irq(hi->det_irq, slp_jack_detect_irq,
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				   IRQF_ONESHOT, "slp_jack_detect", hi);
	if (ret) {
		pr_err("%s : Failed to request_irq.\n", __func__);
		goto err_request_detect_irq;
	}

	/* to handle button when AP sleeping */
	/* Disable earphone button wake-up function temporarily.
	ret = enable_irq_wake(hi->button_irq);
	if (ret) {
		pr_err("%s : Failed to enable_irq_wake for btn.\n", __func__);
		goto err_enable_irq_wake;
	}
	*/

	/* to handle insert/removal when we're sleeping in a call */
	ret = enable_irq_wake(hi->det_irq);
	if (ret) {
		pr_err("%s : Failed to enable_irq_wake.\n", __func__);
		goto err_enable_irq_wake;
	}

	return 0;

err_enable_irq_wake:
	free_irq(hi->det_irq, hi);
err_request_detect_irq:
	free_irq(hi->button_irq, hi);
err_request_detect_btn_irq:
	destroy_workqueue(hi->queue);
err_create_wq_failed:
	gpio_free(pdata->button_gpio);
err_key_gpio_request:
	gpio_free(pdata->det_gpio);
err_det_gpio_request:
	kfree(hi);
err_kzalloc:
	atomic_set(&instantiated, 0);

	return ret;
}

int slp_jack_deinit(void *jack_info)
{
	struct slp_jack_info *hi = jack_info;

	pr_info("%s :\n", __func__);
	disable_irq_wake(hi->det_irq);
	free_irq(hi->button_irq, hi);
	free_irq(hi->det_irq, hi);
	destroy_workqueue(hi->queue);
	gpio_free(hi->pdata->button_gpio);
	gpio_free(hi->pdata->det_gpio);
	kfree(hi);
	atomic_set(&instantiated, 0);

	return 0;
}

MODULE_AUTHOR("kibum.lee@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Corp Ear-Jack detection driver");
MODULE_LICENSE("GPL");
