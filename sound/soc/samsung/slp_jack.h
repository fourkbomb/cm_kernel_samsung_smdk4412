/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
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

#ifndef __ASM_ARCH_SLP_HEADSET_H
#define __ASM_ARCH_SLP_HEADSET_H

#ifdef __KERNEL__

#define SND_JACK_NONE 0

enum snd_jack_btn_state {
	SND_JACK_BTN_RELEASED = 0,
	SND_JACK_BTN_PRESSED = 1
};

enum snd_jack_report_types {
	SND_JACK_BTN = 0,
	SND_JACK_DET = 1
};

struct slp_jack_zone {
	unsigned int adc_high;
	unsigned int delay_ms;
	unsigned int check_count;
	unsigned int jack_type;
};

struct slp_jack_button_zone {
	unsigned int code;
	unsigned int adc_low;
	unsigned int adc_high;
	unsigned int debounce_time;
};

struct slp_jack_platform_data {
	void	(*set_micbias_state) (bool);
	int	(*get_adc_value) (void);
	struct slp_jack_zone	*zones;
	struct slp_jack_button_zone	*button_zones;
	int	num_zones;
	int	num_button_zones;
	int	det_gpio;
	int	button_gpio;
	bool	det_active_high;
	bool	button_active_high;
	void (*jack_mach_init)(void);
};

int slp_jack_init(struct snd_card *card, const char *id, int type,
		struct slp_jack_platform_data *pdata);
int slp_jack_dinit(void *jack_info);

#endif

#endif
