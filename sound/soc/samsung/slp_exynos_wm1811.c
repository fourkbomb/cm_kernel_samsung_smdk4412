/*
 *  slp_exynos_wm1811.c
 *
 *  Copyright (c) 2012 Samsung Electronics Co. Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/jack.h>

#include <asm/mach-types.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>

#include <mach/regs-clock.h>
#include <mach/pmu.h>

#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>

#include <linux/pm.h>
#include <linux/pm_wakeup.h>

#include "i2s.h"
#include "s3c-i2s-v2.h"
#include "../codecs/wm8994.h"

/* For PQ_Proxima Rev00 micbias define */
#define GPIO_MIC_BIAS_EN_00		EXYNOS4212_GPM4(5)
#define GPIO_SUB_MIC_BIAS_EN_00		EXYNOS4212_GPM4(6)

#define XTAL_24MHZ_AP		24000000
#define CODEC_CLK32K		32768
#define CODEC_DEFAULT_SYNC_CLK	11289600

#define WM1811_JACKDET_MODE_NONE  0x0000
#define WM1811_JACKDET_MODE_JACK  0x0100
#define WM1811_JACKDET_MODE_MIC   0x0080
#define WM1811_JACKDET_MODE_AUDIO 0x0180

#define WM1811_JACKDET_BTN0	0x04
#define WM1811_JACKDET_BTN1	0x10
#define WM1811_JACKDET_BTN2	0x08

#define SOC_CARD_NAME_MAX 20


#define SND_SOC_DAPM_SPKMODE(wname, wevent) \
{       .id = snd_soc_dapm_spk, .name = wname, .kcontrol_news = NULL, \
	.num_kcontrols = 0, .reg = SND_SOC_NOPM, .event = wevent, \
	.event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD}

enum {
	MIC_MAIN,
	MIC_SUB,
	MIC_NUM
};

static char *str_mic[] = {
	"Main Mic",
	"Sub Mic",
};

enum {
#if defined(CONFIG_MACH_SLP_PQ_LTE)
	SND_BOARD_PQ_LTE,
#endif
#if defined(CONFIG_MACH_SLP_PQ)
	SND_BOARD_PQ_V00, /* M0_Proxima 00 */
	SND_BOARD_PQ_V01, /* M0_Proxima R2 */
	SND_BOARD_PQ_V06, /* M0 V0.6 */
	SND_BOARD_PQ_V11, /* M0 V1.1 */
#endif
#if defined(CONFIG_MACH_REDWOOD)
	SND_BOARD_REDWOOD_UNIV, /* Redwood Universal board */
	SND_BOARD_REDWOOD_R00, /* Redwood Real rev0.0 */
	SND_BOARD_REDWOOD_R01, /* Redwood Real rev0.1 */
	SND_BOARD_REDWOOD_R02, /* Redwood Real rev0.2 */
#endif
#if defined(CONFIG_MACH_SLP_T0_LTE)
	SND_BOARD_T0_LTE, /* T0 LTE */
#endif
};

static char *str_board[] = {
#if defined(CONFIG_MACH_SLP_PQ_LTE)
	"SLP_PQ_LTE",
#endif
#if defined(CONFIG_MACH_SLP_PQ)
	"SLP_PQ_00",
	"SLP_PQ_R2",
	"SLP_M0_V06",
	"SLP_M0_V11",
#endif
#if defined(CONFIG_MACH_REDWOOD)
	"SLP_REDWOOD_UNIV",
	"SLP_REDWOOD_R00",
	"SLP_REDWOOD_R01",
	"SLP_REDWOOD_R02",
#endif
#if defined(CONFIG_MACH_SLP_T0_LTE)
	"SLP_T0_LTE",
#endif
};
struct slp_snd_board_info {
	unsigned int board_id;
	bool mic_avail[MIC_NUM];
	bool use_ext_mic_bias;
	unsigned int ext_mic_bias[MIC_NUM];
	bool earjack_support;
	bool use_vps_switch;
	unsigned int vps_en_gpio;
};

static struct slp_snd_board_info board_info[] = {
#if defined(CONFIG_MACH_SLP_PQ_LTE)
	{ /* SND_BOARD_PQ_LTE */
		.board_id		= SND_BOARD_PQ_LTE,
		.mic_avail		= { true, false },
		.use_ext_mic_bias	= false,
		.ext_mic_bias		= { 0, 0 },
		.earjack_support	= false,
		.use_vps_switch		= false,
		.vps_en_gpio		= 0,
	},
#endif
#if defined(CONFIG_MACH_SLP_PQ)
	{ /* SND_BOARD_PQ_V00 */
		.board_id		= SND_BOARD_PQ_V00,
		.mic_avail		= { true, true },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN_00,
						GPIO_SUB_MIC_BIAS_EN_00 },
		.earjack_support	= false,
		.use_vps_switch		= false,
		.vps_en_gpio		= 0,
	},
	{ /* SND_BOARD_PQ_V01 */
		.board_id		= SND_BOARD_PQ_V01,
		.mic_avail		= { true, true },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN,
						GPIO_SUB_MIC_BIAS_EN },
		.earjack_support	= false,
		.use_vps_switch		= false,
		.vps_en_gpio		= 0,
	},
	{ /* SND_BOARD_PQ_V06 */
		.board_id		= SND_BOARD_PQ_V06,
		.mic_avail		= { true, true },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN,
						GPIO_SUB_MIC_BIAS_EN },
		.earjack_support	= true,
		.use_vps_switch		= false,
		.vps_en_gpio		= 0,
	},
	{ /* SND_BOARD_PQ_V11 */
		.board_id		= SND_BOARD_PQ_V11,
		.mic_avail		= { true, true },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN,
						GPIO_SUB_MIC_BIAS_EN },
		.earjack_support	= true,
		.use_vps_switch		= false,
		.vps_en_gpio		= 0,
	},
#endif
#if defined(CONFIG_MACH_REDWOOD)
	{ /* SND_BOARD_REDWOOD_UNIV */
		.board_id		= SND_BOARD_REDWOOD_UNIV,
		.mic_avail		= { true, false },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN, 0 },
		.earjack_support	= true,
		.use_vps_switch		= false,
		.vps_en_gpio		= 0,
	},
	{ /* SND_BOARD_REDWOOD_R00 */
		.board_id		= SND_BOARD_REDWOOD_R00,
		.mic_avail		= { true, false },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN, 0 },
		.earjack_support	= true,
		.use_vps_switch		= false,
		.vps_en_gpio		= 0,
	},
	{ /* SND_BOARD_REDWOOD_R01 */
		.board_id		= SND_BOARD_REDWOOD_R01,
		.mic_avail		= { true, false },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN, 0 },
		.earjack_support	= true,
		.use_vps_switch		= true,
		.vps_en_gpio		= GPIO_VPS_SOUND_EN,
	},
	{ /* SND_BOARD_REDWOOD_R02 */
		.board_id		= SND_BOARD_REDWOOD_R02,
		.mic_avail		= { true, false },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN, 0 },
		.earjack_support	= true,
		.use_vps_switch		= true,
		.vps_en_gpio		= GPIO_VPS_SOUND_EN,
	},
#endif
#if defined(CONFIG_MACH_SLP_T0_LTE)
	{ /* SND_BOARD_T0_LTE */
		.board_id		= SND_BOARD_T0_LTE,
		.mic_avail		= { true, true },
		.use_ext_mic_bias	= true,
		.ext_mic_bias		= { GPIO_MIC_BIAS_EN,
						GPIO_SUB_MIC_BIAS_EN },
		.earjack_support	= false,
		.use_vps_switch		= false,
		.vps_en_gpio		= 0,
	},
#endif
};

static struct slp_snd_board_info *this_board;

static const struct wm8958_micd_rate machine_det_rates[] = {
	{ CODEC_CLK32K,			true,  0, 0 },
	{ CODEC_CLK32K,			false, 0, 0 },
	{ CODEC_DEFAULT_SYNC_CLK,	true,  0xa, 0xb },
	{ CODEC_DEFAULT_SYNC_CLK,	false, 0xa, 0xb },
};

static const struct wm8958_micd_rate machine_jackdet_rates[] = {
	{ CODEC_CLK32K,			true,  0, 0 },
	{ CODEC_CLK32K,			false, 0, 0 },
	{ CODEC_DEFAULT_SYNC_CLK,	true, 12, 12 },
	{ CODEC_DEFAULT_SYNC_CLK,	false, 7, 7 },
};

static int aif2_mode;
const char *aif2_mode_text[] = {
	"Slave", "Master"
};
enum {
	MODE_CODEC_SLAVE,
	MODE_CODEC_MASTER,
};

static struct platform_device *slp_snd_device;

/* To support PBA function test */
static struct class *audio_class;
static struct device *jack_dev;
static struct device *caps_dev;

static int exynos_snd_mclk_use_cnt;
static bool exynos_snd_mclk_enabled;

struct wm1811_machine_priv {
	struct snd_soc_jack jack;
	struct snd_soc_codec *codec;
	struct delayed_work mic_work;
};

static int check_board(void)
{
#if defined(CONFIG_MACH_SLP_PQ_LTE)
	if (machine_is_slp_pq_lte())
		return SND_BOARD_PQ_LTE;
#endif
#if defined(CONFIG_MACH_SLP_PQ)
	if (machine_is_slp_pq()) {
		switch (system_rev & 0xff) {
		case 0x03:
			return SND_BOARD_PQ_V00;
		case 0x00: /* R2 */
			return SND_BOARD_PQ_V01;
		case 0x07: /* Type B */
		case 0x08: /* Type A */
			return SND_BOARD_PQ_V06;
		case 0x0c:
		default:
			return SND_BOARD_PQ_V11;
		}
	}
#endif
#if defined(CONFIG_MACH_REDWOOD)
	if (machine_is_redwood()) {
		switch (system_rev & 0xff) {
		case 0x0c:
		case 0x04: /* universal board */
			return SND_BOARD_REDWOOD_UNIV;
		case 0x06: /* real rev 0.0 */
			return SND_BOARD_REDWOOD_R00;
		case 0x07: /* real rev 0.1 */
			return SND_BOARD_REDWOOD_R01;
		case 0x08: /* real rev 0.2 */
		default: /* assume newer board as real 0.2 */
			return SND_BOARD_REDWOOD_R02;
		}
	}
#endif
#if defined(CONFIG_MACH_SLP_T0_LTE)
	if (machine_is_t0_lte())
		return SND_BOARD_T0_LTE;
#endif
	return -1;
}

static void exynos_snd_set_mclk_forced(bool on)
{
	if (on) {
		/* enable mclk */
		dev_info(&slp_snd_device->dev, "forced enable mclk");
		exynos4_pmu_xclkout_set(1, XCLKOUT_XUSBXTI);

		exynos_snd_mclk_use_cnt++;
		exynos_snd_mclk_enabled = true;
		mdelay(10);
	} else {
		/* disable mclk */
		dev_info(&slp_snd_device->dev, "forced disable mclk");
		exynos4_pmu_xclkout_set(0, XCLKOUT_XUSBXTI);

		exynos_snd_mclk_use_cnt = 0;
		exynos_snd_mclk_enabled = false;
	}
	dev_info(&slp_snd_device->dev, "enabled: %d, count: %d",
			exynos_snd_mclk_enabled, exynos_snd_mclk_use_cnt);
}

static void exynos_snd_set_mclk(bool on)
{
	if (on)
		exynos_snd_mclk_use_cnt++;
	else {
		exynos_snd_mclk_use_cnt--;
		if (exynos_snd_mclk_use_cnt < 0)
			exynos_snd_mclk_use_cnt = 0;
	}

	if (exynos_snd_mclk_use_cnt) {
		if (!exynos_snd_mclk_enabled) {
			/* enable mclk */
			dev_info(&slp_snd_device->dev, "enable mclk");
			exynos_snd_mclk_enabled = true;
			exynos4_pmu_xclkout_set(exynos_snd_mclk_enabled,
						XCLKOUT_XUSBXTI);
			mdelay(10);
		} else {
			/* already enabled */
			dev_info(&slp_snd_device->dev, "mclk already enabled");
		}
	} else {
		if (exynos_snd_mclk_enabled) {
			/* disable mclk */
			dev_info(&slp_snd_device->dev, "disable mclk");
			exynos_snd_mclk_enabled = false;
			exynos4_pmu_xclkout_set(exynos_snd_mclk_enabled,
						XCLKOUT_XUSBXTI);
		} else {
			/* already disabled */
			dev_info(&slp_snd_device->dev, "mclk already disabled");
		}

	}
	dev_info(&slp_snd_device->dev, "enabled: %d, count: %d",
			exynos_snd_mclk_enabled, exynos_snd_mclk_use_cnt);
}

static void exynos_snd_gpio_init(void)
{
	int err;
	int i;

	if (!this_board->use_ext_mic_bias)
		return;

	for (i = MIC_MAIN; i < MIC_NUM; i++) {
		if (this_board->mic_avail[i]) {
			err = gpio_request(this_board->ext_mic_bias[i],
								str_mic[i]);
			if (err) {
				dev_err(&slp_snd_device->dev,
					"%s GPIO set error!", str_mic[i]);
				return;
			}
			dev_info(&slp_snd_device->dev,
						"%s GPIO init", str_mic[i]);
			gpio_direction_output(this_board->ext_mic_bias[i], 0);
			gpio_free(this_board->ext_mic_bias[i]);
		}
	}
}

static const struct soc_enum aif2_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif2_mode_text), aif2_mode_text),
};

static int get_aif2_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = aif2_mode;
	return 0;
}

static int set_aif2_mode(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	if (aif2_mode == ucontrol->value.integer.value[0])
		return 0;

	aif2_mode = ucontrol->value.integer.value[0];

	BUG_ON(!((aif2_mode == MODE_CODEC_SLAVE)
			|| (aif2_mode == MODE_CODEC_MASTER)));
	pr_info("set AIF2 mode: %s\n", aif2_mode_text[aif2_mode]);
	return 0;
}

static int exynos_snd_micbias(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned int mic;

	dev_info(codec->dev, "%s event is %02X", w->name, event);

	if (!strncmp(w->name, str_mic[MIC_MAIN], strlen(str_mic[MIC_MAIN])))
		mic = MIC_MAIN;
	else if (!strncmp(w->name, str_mic[MIC_SUB], strlen(str_mic[MIC_SUB])))
		mic = MIC_SUB;
	else {
		dev_err(&slp_snd_device->dev,
					"Unknown dapm widget %s\n", w->name);
		return 0;
	}

	if (!this_board->mic_avail[mic]) {
		dev_info(&slp_snd_device->dev, "%s does not exist on this board",
								str_mic[mic]);
		return 0;
	}


	if (this_board->use_ext_mic_bias) {
		switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			dev_info(&slp_snd_device->dev,
					"%s bias enable", str_mic[mic]);
			gpio_set_value(this_board->ext_mic_bias[mic], 1);
			/* add delay to remove main mic pop up noise */
			msleep(150);
			break;
		case SND_SOC_DAPM_POST_PMD:
			dev_info(&slp_snd_device->dev,
					"%s bias disable", str_mic[mic]);
			gpio_set_value(this_board->ext_mic_bias[mic], 0);
			break;
		}
	} else {
		/* MICBIAS1 (A9) pin is used for both main & sub mic bias */
		switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
			dev_info(&slp_snd_device->dev,
					"%s bias enable", str_mic[mic]);
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
				WM8994_MICB1_ENA_MASK, WM8994_MICB1_ENA);
			break;
		case SND_SOC_DAPM_POST_PMD:
			dev_info(&slp_snd_device->dev,
					"%s bias disable", str_mic[mic]);
			snd_soc_update_bits(codec, WM8994_POWER_MANAGEMENT_1,
				WM8994_MICB1_ENA_MASK, 0);
			break;
		}
	}

	return 0;
}

static int exynos_snd_spkmode(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_info(codec->dev, "set speaker L+R mix");
		ret = snd_soc_update_bits(codec, WM8994_SPKOUT_MIXERS,
					  WM8994_SPKMIXR_TO_SPKOUTL_MASK,
					  WM8994_SPKMIXR_TO_SPKOUTL);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_info(codec->dev, "unset speaker L+R mix");
		ret = snd_soc_update_bits(codec, WM8994_SPKOUT_MIXERS,
					  WM8994_SPKMIXR_TO_SPKOUTL_MASK,
					  0);
		break;
	}

	return ret;
}

static int exynos_snd_vps_switch(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	if (this_board->use_vps_switch) {
		switch (event) {
		case SND_SOC_DAPM_POST_PMU:
			dev_info(&slp_snd_device->dev, "vps switch enable");
			gpio_set_value(this_board->vps_en_gpio, 1);
			break;
		case SND_SOC_DAPM_PRE_PMD:
			dev_info(&slp_snd_device->dev, "vps switch disable");
			gpio_set_value(this_board->vps_en_gpio, 0);
			break;
		}
	}

	return 0;
}

static void codec_micd_set_rate(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	int best, i, sysclk, val;
	bool idle;
	const struct wm8958_micd_rate *rates = NULL;
	int num_rates = 0;

	idle = !wm8994->jack_mic;

	sysclk = snd_soc_read(codec, WM8994_CLOCKING_1);
	if (sysclk & WM8994_SYSCLK_SRC)
		sysclk = wm8994->aifclk[1];
	else
		sysclk = wm8994->aifclk[0];

	if (wm8994->jackdet) {
		rates = machine_jackdet_rates;
		num_rates = ARRAY_SIZE(machine_jackdet_rates);
	} else {
		rates = machine_det_rates;
		num_rates = ARRAY_SIZE(machine_det_rates);
	}

	best = 0;
	for (i = 0; i < num_rates; i++) {
		if (rates[i].idle != idle)
			continue;
		if (abs(rates[i].sysclk - sysclk) <
		    abs(rates[best].sysclk - sysclk))
			best = i;
		else if (rates[best].idle != idle)
			best = i;
	}

	val = rates[best].start << WM8958_MICD_BIAS_STARTTIME_SHIFT
		| rates[best].rate << WM8958_MICD_RATE_SHIFT;

	snd_soc_update_bits(codec, WM8958_MIC_DETECT_1,
			    WM8958_MICD_BIAS_STARTTIME_MASK |
			    WM8958_MICD_RATE_MASK, val);

#if defined(CONFIG_MACH_SLP_PQ)
	/*
	 * Some revision of M0_PROXIMA does not have
	 * external jackdet pin even though using wm1811 codec.
	 * So if codec is not wm1811a - which supports
	 * internal jackdet - increase debounce time.
	 */
	if (!wm8994->jackdet && this_board->earjack_support)
		snd_soc_update_bits(codec, WM8958_MIC_DETECT_1,
				    WM8958_MICD_DBTIME_MASK,
				    WM8958_MICD_DBTIME);
#endif
}

static void codec_micdet(u16 status, void *data)
{
	struct wm1811_machine_priv *wm1811 = data;
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(wm1811->codec);
	int report;
	static int check_report;


	dev_info(&slp_snd_device->dev, "jack status 0x%x", status);

	/* LCD is still off even though earjack event has occurred.
	   So platform daemon need time to deal this event */
	pm_wakeup_event(wm1811->codec->dev, 500);

	/* Either nothing present or just starting detection */
	if (!(status & WM8958_MICD_STS)) {
		if (!wm8994->jackdet) {
			/* If nothing present then clear our statuses */
			dev_dbg(wm1811->codec->dev, "Detected open circuit\n");
			wm8994->jack_mic = false;
			wm8994->mic_detecting = true;

			codec_micd_set_rate(wm1811->codec);

			snd_soc_jack_report(wm8994->micdet[0].jack, 0,
					    wm8994->btn_mask |
					     SND_JACK_HEADSET);
		}
	}

	/* If the measurement is showing a high impedence we've got a
	 * microphone.
	 */
	if (wm8994->mic_detecting && (status & 0x400)) {
		dev_info(wm1811->codec->dev, "Detected microphone\n");

		wm8994->mic_detecting = false;
		wm8994->jack_mic = true;

		codec_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack,
				SND_JACK_HEADSET, SND_JACK_HEADSET);
	}

	if (wm8994->mic_detecting && status & 0x4) {
		dev_info(wm1811->codec->dev, "Detected headphone\n");
		wm8994->mic_detecting = false;

		codec_micd_set_rate(wm1811->codec);

		snd_soc_jack_report(wm8994->micdet[0].jack,
				SND_JACK_HEADPHONE, SND_JACK_HEADSET);

		/* If we have jackdet that will detect removal */
		if (wm8994->jackdet) {
			mutex_lock(&wm8994->accdet_lock);

			snd_soc_update_bits(wm1811->codec,
					WM8958_MIC_DETECT_1,
					WM8958_MICD_ENA, 0);

			if (wm8994->active_refcount) {
				snd_soc_update_bits(wm1811->codec,
					WM8994_ANTIPOP_2,
					WM1811_JACKDET_MODE_MASK,
					WM1811_JACKDET_MODE_AUDIO);
			}

			mutex_unlock(&wm8994->accdet_lock);

			if (wm8994->pdata->jd_ext_cap) {
				mutex_lock(&wm1811->codec->mutex);
				snd_soc_dapm_disable_pin(&wm1811->codec->dapm,
							"MICBIAS2");
				snd_soc_dapm_sync(&wm1811->codec->dapm);
				mutex_unlock(&wm1811->codec->mutex);
			}

		}
	}

	/* Report short circuit as a button */
	if (wm8994->jack_mic) {
		report = 0;
		if (status & WM1811_JACKDET_BTN0)
			report |= SND_JACK_BTN_0;

		if (status & WM1811_JACKDET_BTN1)
			report |= SND_JACK_BTN_1;

		if (status & WM1811_JACKDET_BTN2)
			report |= SND_JACK_BTN_2;

		/* TODO : Check this on wm1811a */
		if (check_report != report) {
			dev_info(&slp_snd_device->dev,
				"Report Button: %08x (%08X)", report, status);

			snd_soc_jack_report(wm8994->micdet[0].jack, report,
					    wm8994->btn_mask);
			check_report = report;
		} else
			dev_info(&slp_snd_device->dev, "Skip button report");
	}
}

#ifdef CONFIG_SND_SAMSUNG_I2S_MASTER
static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}
#endif /* CONFIG_SND_SAMSUNG_I2S_MASTER */

#ifndef CONFIG_SND_SAMSUNG_I2S_MASTER
static int machine_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int ret;

	/* AIF1CLK should be >=3MHz for optimal performance */
	if (params_rate(params) == 8000 || params_rate(params) == 11025)
		pll_out = params_rate(params) * 512;
	else
		pll_out = params_rate(params) * 256;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set the cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;


	/* Switch FLL */
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1,
				WM8994_FLL_SRC_MCLK1, XTAL_24MHZ_AP,
				pll_out);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to start FLL1: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
					pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(codec_dai->dev,
				"Unable to switch to FLL1: %d\n", ret);
		return ret;
	}

	/*
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK, 0,
					MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;
	*/

	codec_micd_set_rate(codec_dai->codec);

	return 0;
}
#else /* CONFIG_SND_SAMSUNG_I2S_MASTER */
static int machine_aif1_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int bfs, psr, rfs, ret;
	unsigned long rclk;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk(KERN_INFO "Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_MCLK1,
					rclk, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

	return 0;
}
#endif /* CONFIG_SND_SAMSUNG_I2S_MASTER */

/*
 * Midas WM1811 DAI operations.
 */
static struct snd_soc_ops machine_aif1_ops = {
	.hw_params = machine_aif1_hw_params,
};

static int machine_aif2_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;
	int prate;
	int bclk;
	unsigned int fmt;
	int pll_src;
	unsigned int pll_freq_in;

	prate = params_rate(params);
	switch (params_rate(params)) {
	case 8000:
	case 16000:
	       break;
	default:
		dev_warn(codec_dai->dev, "Unsupported LRCLK %d, falling back to 8000Hz\n",
				(int)params_rate(params));
		prate = 8000;
	}

#ifdef CONFIG_LTE_MODEM_CMC221
	fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_IB_NF;
#else
	fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF;
#endif
	if (aif2_mode == MODE_CODEC_SLAVE)
		fmt |= SND_SOC_DAIFMT_CBS_CFS;
	else /* MODE_CODEC_MASTER */
		fmt |= SND_SOC_DAIFMT_CBM_CFM;

	/* Set the codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);

	if (ret < 0)
		return ret;

#ifdef CONFIG_LTE_MODEM_CMC221
	bclk = 2048000;
#else
	switch (prate) {
	case 8000:
		bclk = 256000;
		break;
	case 16000:
		bclk = 512000;
		break;
	default:
		return -EINVAL;
	}
#endif

	if (aif2_mode == MODE_CODEC_SLAVE) {
		pll_src = WM8994_FLL_SRC_BCLK;
		pll_freq_in = bclk;
	} else {
		pll_src = WM8994_FLL_SRC_MCLK1;
		pll_freq_in = XTAL_24MHZ_AP;
	}
	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL2,
				pll_src, pll_freq_in,
				prate * 256);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to configure FLL2: %d\n", ret);

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL2,
					prate * 256, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec_dai->dev, "Unable to switch to FLL2: %d\n", ret);

#if defined(CONFIG_MACH_SLP_PQ)
	/*
	 * Currently CP audio is only on left channel.
	 * Use AIF2 DAC Filter's mono mix control
	 * to make duplicated mono audio stream.
	 * This is tested only in M0_Proxima REV01
	 */
	if (this_board->board_id == SND_BOARD_PQ_V01 && rtd->codec) {
		ret = snd_soc_update_bits(rtd->codec, WM8994_AIF2_DAC_FILTERS_1,
					WM8994_AIF2DAC_MONO_MASK,
					1 << WM8994_AIF2DAC_MONO_SHIFT);
		if (ret > 0)
			dev_err(codec_dai->dev, "AIF2DAC_MONO updated\n");
		else if (ret == 0)
			dev_err(codec_dai->dev, "AIF2DAC_MONO no change\n");
		else
			dev_err(codec_dai->dev, "AIF2DAC_MONO failed\n");
	}
#endif

	return 0;
}

static struct snd_soc_ops machine_aif2_ops = {
	.hw_params = machine_aif2_hw_params,
};

static int machine_aif3_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	dev_err(&slp_snd_device->dev, "%s: enter", __func__);
	return 0;
}

static struct snd_soc_ops machine_aif3_ops = {
	.hw_params = machine_aif3_hw_params,
};

static const struct snd_kcontrol_new machine_controls[] = {
	SOC_DAPM_PIN_SWITCH("HP"),
	SOC_DAPM_PIN_SWITCH("SPK"),
	SOC_DAPM_PIN_SWITCH("RCV"),
	SOC_DAPM_PIN_SWITCH("LINE"),
	SOC_DAPM_PIN_SWITCH("HDMI"),

	SOC_DAPM_PIN_SWITCH("Main Mic"),
	SOC_DAPM_PIN_SWITCH("Sub Mic"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),

	SOC_ENUM_EXT("AIF2 Mode", aif2_mode_enum[0],
		get_aif2_mode, set_aif2_mode),
};

const struct snd_soc_dapm_widget machine_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_SPKMODE("SPK", exynos_snd_spkmode),
	SND_SOC_DAPM_SPK("RCV", NULL),
	SND_SOC_DAPM_LINE("LINE", exynos_snd_vps_switch),
	SND_SOC_DAPM_LINE("HDMI", NULL),

	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Main Mic", exynos_snd_micbias),
	SND_SOC_DAPM_MIC("Sub Mic", exynos_snd_micbias),
};

const struct snd_soc_dapm_route machine_dapm_routes[] = {
	{ "HP", NULL, "HPOUT1L" },
	{ "HP", NULL, "HPOUT1R" },

	{ "SPK", NULL, "SPKOUTLN" },
	{ "SPK", NULL, "SPKOUTLP" },
	{ "SPK", NULL, "SPKOUTRN" },
	{ "SPK", NULL, "SPKOUTRP" },

	{ "RCV", NULL, "HPOUT2N" },
	{ "RCV", NULL, "HPOUT2P" },

	{ "LINE", NULL, "LINEOUT2N" },
	{ "LINE", NULL, "LINEOUT2P" },

	{ "HDMI", NULL, "LINEOUT1N" }, /* Not connected */
	{ "HDMI", NULL, "LINEOUT1P" }, /* Not connected */

	{ "IN1LP", NULL, "Main Mic" },
	{ "IN1LN", NULL, "Main Mic" },

	{ "IN1RP", NULL, "Sub Mic" },
	{ "IN1RN", NULL, "Sub Mic" },

	{ "IN2LP:VXRN", NULL, "MICBIAS2" },
	{ "MICBIAS2", NULL, "Headset Mic" },
};

#if defined(CONFIG_MACH_REDWOOD)
const struct snd_soc_dapm_route machine_dapm_routes_ex[] = {
	{ "IN1RP", NULL, "MICBIAS2" },
	{ "IN1RN", NULL, "MICBIAS2" },
};
#endif

static void wm1811_mic_work(struct work_struct *work)
{
	int report = 0;
	struct wm1811_machine_priv *wm1811;
	struct snd_soc_codec *codec;
	int status;

	wm1811 = container_of(work, struct wm1811_machine_priv,
							mic_work.work);
	codec = wm1811->codec;

	status = snd_soc_read(codec, WM8958_MIC_DETECT_3);
	if (status < 0) {
		dev_err(codec->dev, "Failed to read mic detect status: %d\n",
				status);
		return;
	}

	/* If nothing present then clear our statuses */
	if (!(status & WM8958_MICD_STS))
		goto done;

	report = SND_JACK_HEADSET;

	/* Everything else is buttons; just assign slots */
	if (status & WM1811_JACKDET_BTN0)
		report |= SND_JACK_BTN_0;
	if (status & WM1811_JACKDET_BTN1)
		report |= SND_JACK_BTN_1;
	if (status & WM1811_JACKDET_BTN2)
		report |= SND_JACK_BTN_2;

	if (report & SND_JACK_MICROPHONE)
		dev_crit(codec->dev, "Reporting microphone\n");
	if (report & SND_JACK_HEADPHONE)
		dev_crit(codec->dev, "Reporting headphone\n");
	if (report & SND_JACK_BTN_0)
		dev_crit(codec->dev, "Reporting button 0\n");
	if (report & SND_JACK_BTN_1)
		dev_crit(codec->dev, "Reporting button 1\n");
	if (report & SND_JACK_BTN_2)
		dev_crit(codec->dev, "Reporting button 2\n");

done:
	if (!report)
		dev_crit(codec->dev, "Reporting open circuit\n");

	snd_soc_jack_report(&wm1811->jack, report,
				SND_JACK_BTN_0 | SND_JACK_BTN_1 |
				SND_JACK_BTN_2 | SND_JACK_HEADSET);
}

static struct snd_soc_dai_driver machine_ext_dai[] = {
	{
		.name = "machine.cp",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "machine.bt",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 16000,
			.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
};

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	dev_info(&slp_snd_device->dev, "%s: operate nothing", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	int report = 0;

	wm8994->mic_detecting = false;
	wm8994->jack_mic = true;

	codec_micd_set_rate(codec);

	if ((!size) || (buf[0] == '0')) {
		snd_soc_jack_report(wm8994->micdet[0].jack,
				    0, SND_JACK_HEADSET);
		dev_info(codec->dev, "Forced remove earjack\n");
	} else {
		if (buf[0] == '1') {
			dev_info(codec->dev, "Forced detect microphone\n");
			report = SND_JACK_HEADSET;
		} else {
			dev_info(codec->dev, "Forced detect headphone\n");
			report = SND_JACK_HEADPHONE;
		}
		snd_soc_jack_report(wm8994->micdet[0].jack,
				    report, SND_JACK_HEADSET);
	}

	return size;
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);


static ssize_t audio_caps_cp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!this_board)
		return 0;

	switch (this_board->board_id) {
#if defined(CONFIG_MACH_SLP_PQ_LTE)
	case SND_BOARD_PQ_LTE:
		return sprintf(buf, "nb\n") + 1;
#endif
#if defined(CONFIG_MACH_SLP_PQ)
	case SND_BOARD_PQ_V00:
	case SND_BOARD_PQ_V01:
	case SND_BOARD_PQ_V06:
	case SND_BOARD_PQ_V11:
		return sprintf(buf, "nb\n") + 1;
#endif
#if defined(CONFIG_MACH_REDWOOD)
	case SND_BOARD_REDWOOD_UNIV:
		if ((system_rev & 0xff) == 0x0c)
			return sprintf(buf, "nb\n") + 1;
	case SND_BOARD_REDWOOD_R00:
	case SND_BOARD_REDWOOD_R01:
	case SND_BOARD_REDWOOD_R02:
		return sprintf(buf, "wb\n") + 1;
#endif
#if defined(CONFIG_MACH_SLP_T0_LTE)
	case SND_BOARD_T0_LTE:
#endif
	default:
		return sprintf(buf, "nb\n") + 1;
	}
}
static DEVICE_ATTR(cp_caps, S_IRUGO, audio_caps_cp_show, NULL);

static int machine_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct wm1811_machine_priv *wm1811
		= snd_soc_card_get_drvdata(codec->card);
	struct snd_soc_dai *aif1_dai = rtd->codec_dai;
	struct wm8994 *wm8994 = codec->control_data;
	int ret;

	exynos_snd_set_mclk(true);

	ret = snd_soc_add_controls(codec, machine_controls,
					ARRAY_SIZE(machine_controls));

	ret = snd_soc_dapm_new_controls(&codec->dapm, machine_dapm_widgets,
					   ARRAY_SIZE(machine_dapm_widgets));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add DAPM widgets: %d\n", ret);

	ret = snd_soc_dapm_add_routes(&codec->dapm, machine_dapm_routes,
					   ARRAY_SIZE(machine_dapm_routes));
	if (ret != 0)
		dev_err(codec->dev, "Failed to add DAPM routes: %d\n", ret);

#if defined(CONFIG_MACH_REDWOOD)
	if ((machine_is_redwood()) &&
		(this_board->board_id >= SND_BOARD_REDWOOD_R00)) {
		ret = snd_soc_dapm_add_routes(&codec->dapm,
			machine_dapm_routes_ex,
			ARRAY_SIZE(machine_dapm_routes_ex));

		if (ret != 0)
			dev_err(codec->dev,
				"Failed to add extended DAPM routes:%d\n", ret);
	}
#endif

	ret = snd_soc_dai_set_sysclk(aif1_dai, WM8994_SYSCLK_MCLK2,
				     CODEC_CLK32K, SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to boot clocking\n");

	/* Force AIF1CLK on as it will be master for jack detection */
	ret = snd_soc_dapm_force_enable_pin(&codec->dapm, "AIF1CLK");
	if (ret < 0)
		dev_err(codec->dev,
			"Failed to enable AIF1CLK: %d\n", ret);

	if (this_board && !this_board->mic_avail[MIC_SUB])
		snd_soc_dapm_nc_pin(&codec->dapm, "Sub Mic");
	if (this_board && !this_board->earjack_support)
		snd_soc_dapm_nc_pin(&codec->dapm, "Headset Mic");
	if (this_board && this_board->use_ext_mic_bias)
		snd_soc_dapm_nc_pin(&codec->dapm, "MICBIAS1");

	/* disable fm raido analog input */
	snd_soc_dapm_nc_pin(&codec->dapm, "IN2RN");
	snd_soc_dapm_nc_pin(&codec->dapm, "IN2RP:VXRP");

	snd_soc_dapm_ignore_suspend(&codec->dapm, "RCV");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "SPK");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "HP");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "Sub Mic");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "Main Mic");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1DACDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2DACDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3DACDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF1ADCDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF2ADCDAT");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "AIF3ADCDAT");

	wm1811->codec = codec;

	codec_micd_set_rate(codec);

	INIT_DELAYED_WORK(&wm1811->mic_work, wm1811_mic_work);

	ret = snd_soc_jack_new(codec, "Headset Jack",
				SND_JACK_HEADSET | SND_JACK_BTN_0 |
				SND_JACK_BTN_1 | SND_JACK_BTN_2,
				&wm1811->jack);

	if (ret < 0)
		dev_err(codec->dev, "Failed to create jack: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_0, KEY_MEDIA);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_MEDIA: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_1,
							KEY_VOLUMEDOWN);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEUP: %d\n", ret);

	ret = snd_jack_set_key(wm1811->jack.jack, SND_JACK_BTN_2,
							KEY_VOLUMEUP);
	if (ret < 0)
		dev_err(codec->dev, "Failed to set KEY_VOLUMEDOWN: %d\n", ret);

	/* TODO : check this on fast boot environment */
	wm1811->jack.status = 0;

	if (this_board && this_board->earjack_support
					&& (wm8994->revision > 1)) {
		ret = wm8958_mic_detect(codec, &wm1811->jack,
						codec_micdet, wm1811);
		if (ret < 0)
			dev_err(codec->dev,
					"Failed start detection: %d\n", ret);
	} else {
		dev_info(codec->dev,
			"Earjack event doesn't support for this board\n");
		codec->dapm.idle_bias_off = 0;
	}

	/* To wakeup for earjack event in suspend mode */
	enable_irq_wake(wm8994->irq);
#ifdef CONFIG_SLP_WAKEUP_COUNT
	device_init_wakeup_setirq(wm1811->codec->dev, wm8994->irq);
#else
	device_init_wakeup(wm1811->codec->dev, true);
#endif

	/* To support PBA function test */
	audio_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(audio_class))
		pr_err("Failed to create class\n");

	jack_dev = device_create(audio_class, NULL, 0, codec, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	caps_dev = device_create(audio_class, NULL, 0, NULL, "caps");

	if (device_create_file(caps_dev, &dev_attr_cp_caps) < 0)
		pr_err("Failed to create device file %s)!\n",
			dev_attr_cp_caps.attr.name);

	return snd_soc_dapm_sync(&codec->dapm);
}

static struct snd_soc_dai_link machine_dai[] = {
	{ /* Primary DAI i/f */
		.name = "WM1811 AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "wm8994-aif1",
		.platform_name = "samsung-audio",
		.codec_name = "wm8994-codec",
		.init = machine_init_paiftx,
		.ops = &machine_aif1_ops,
	},
	{
		.name = "WM1811 Voice",
		.stream_name = "Voice Tx/Rx",
		.cpu_dai_name = "machine.cp",
		.codec_dai_name = "wm8994-aif2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "wm8994-codec",
		.ops = &machine_aif2_ops,
		.ignore_suspend = 1,
	},
	{
		.name = "WM1811 BT",
		.stream_name = "BT Tx/Rx",
		.cpu_dai_name = "machine.bt",
		.codec_dai_name = "wm8994-aif3",
		.platform_name = "snd-soc-dummy",
		.codec_name = "wm8994-codec",
		.ops = &machine_aif3_ops,
		.ignore_suspend = 1,
	},
	{ /* Sec_Fifo DAI i/f */
		.name = "Sec_FIFO TX",
		.stream_name = "Sec_Dai",
		.cpu_dai_name = "samsung-i2s.4",
		.codec_dai_name = "wm8994-aif1",
#ifndef CONFIG_SND_SOC_SAMSUNG_USE_DMA_WRAPPER
		.platform_name = "samsung-audio-idma",
#else
		.platform_name = "samsung-audio",
#endif
		.codec_name = "wm8994-codec",
		.ops = &machine_aif1_ops,
	},
};

static int machine_card_suspend_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	struct snd_soc_dai *aif2_dai = card->rtd[1].codec_dai;
	int ret;

	if (!codec->active) {
		/* change sysclk from pll out of mclk1 to mclk2
		   to disable mclk1 from AP */
		dev_info(codec->dev, "use mclk2 and disable fll");
		ret = snd_soc_dai_set_sysclk(aif2_dai,
						WM8994_SYSCLK_MCLK2,
						CODEC_CLK32K,
						SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(codec->dev,
			"Unable to switch aif2 sysclk to MCLK2: %d\n", ret);

		ret = snd_soc_dai_set_pll(aif2_dai, WM8994_FLL2, 0, 0, 0);
		if (ret < 0)
			dev_err(codec->dev,
				"Unable to stop FLL2: %d\n", ret);

		ret = snd_soc_dai_set_sysclk(aif1_dai,
						WM8994_SYSCLK_MCLK2,
						CODEC_CLK32K,
						SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(codec->dev,
			"Unable to switch aif1 sysclk to MCLK2: %d\n", ret);

		ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1, 0, 0, 0);
		if (ret < 0)
			dev_err(codec->dev, "Unable to stop FLL1: 0x%x\n", ret);

		dev_info(codec->dev, "turn off mclk1");
		exynos_snd_set_mclk_forced(false);
	}

	exynos4_sys_powerdown_xusbxti_control(exynos_snd_mclk_enabled ? 1 : 0);

	snd_soc_dapm_sync(&codec->dapm);

	return 0;
}

static int machine_card_resume_pre(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = card->rtd->codec;
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;
	int ret;
	int reg = 0;

	/* enable mclk1 */
	exynos_snd_set_mclk_forced(true);

	/* change sysclk from mclk2 to pll out of mclk1 */
	ret = snd_soc_dai_set_pll(aif1_dai, WM8994_FLL1,
					WM8994_FLL_SRC_MCLK1,
					XTAL_24MHZ_AP,
					CODEC_DEFAULT_SYNC_CLK);
	if (ret < 0)
		dev_err(aif1_dai->dev, "Unable to start FLL1: 0x%x\n", ret);

	ret = snd_soc_dai_set_sysclk(aif1_dai, WM8994_SYSCLK_FLL1,
					CODEC_DEFAULT_SYNC_CLK,
					SND_SOC_CLOCK_IN);
	if (ret < 0)
		dev_err(aif1_dai->dev,
			"Unable to switch sysclk to FLL1: 0x%x\n", ret);

	codec_micd_set_rate(aif1_dai->codec);

	/* workaround for jack detection
	 * sometimes WM8994_GPIO_1 type changed wrong function type
	 * so if type mismatched, update to IRQ type
	 */
	reg = snd_soc_read(codec, WM8994_GPIO_1);

	if ((reg & WM8994_GPN_FN_MASK) != WM8994_GP_FN_IRQ) {
		dev_err(codec->dev, "%s: GPIO1 type 0x%x\n", __func__, reg);
		snd_soc_write(codec, WM8994_GPIO_1, WM8994_GP_FN_IRQ);
	}

	return 0;
}

static char soc_card_name[SOC_CARD_NAME_MAX] = "wm1811";

static struct snd_soc_card exynos_wm1811_machine = {
	.name = soc_card_name,
	.dai_link = machine_dai,

	/* If you want to use sec_fifo device,
	 * changes the num_link = 2 or ARRAY_SIZE(machine_dai). */
	.num_links = ARRAY_SIZE(machine_dai),
	.suspend_post = machine_card_suspend_post,
	.resume_pre = machine_card_resume_pre
};


static int __init machine_audio_init(void)
{
	struct wm1811_machine_priv *wm1811;
	int ret;
	int rev;


	wm1811 = kzalloc(sizeof *wm1811, GFP_KERNEL);
	if (!wm1811) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_kzalloc;
	}
	snd_soc_card_set_drvdata(&exynos_wm1811_machine, wm1811);

	rev = check_board();
	if (rev < 0) {
		pr_err("%s: Undefined board revision\n", __func__);
		ret = -ENODEV;
		goto err_device_alloc;
	} else {
		pr_info("%s: board type is %s\n", __func__, str_board[rev]);
		this_board = &board_info[rev];
	}

#if defined(CONFIG_MACH_REDWOOD)
	if ((machine_is_redwood()) && (rev >= SND_BOARD_REDWOOD_R00)) {
		memset(soc_card_name, 0, sizeof(soc_card_name));
		strcpy(soc_card_name, "redwood");
	}
#endif

	slp_snd_device = platform_device_alloc("soc-audio", -1);
	if (!slp_snd_device) {
		ret = -ENOMEM;
		goto err_device_alloc;
	}

	slp_snd_device->dev.init_name = slp_snd_device->name;

	ret = snd_soc_register_dais(&slp_snd_device->dev,
				machine_ext_dai, ARRAY_SIZE(machine_ext_dai));
	if (ret != 0)
		pr_err("Audio: Failed to register external DAIs: %d\n", ret);

	platform_set_drvdata(slp_snd_device, &exynos_wm1811_machine);

	ret = platform_device_add(slp_snd_device);
	if (ret)
		platform_device_put(slp_snd_device);

	exynos_snd_gpio_init();

	return ret;

err_device_alloc:
	kfree(wm1811);
err_kzalloc:
	return ret;
}
module_init(machine_audio_init);

static void __exit machine_audio_exit(void)
{
	struct snd_soc_card *card = &exynos_wm1811_machine;
	struct wm1811_machine_priv *wm1811 = snd_soc_card_get_drvdata(card);
	platform_device_unregister(slp_snd_device);
	kfree(wm1811);
}
module_exit(machine_audio_exit);

MODULE_AUTHOR("KwangHui Cho <kwanghui.cho@samsung.com>");
MODULE_DESCRIPTION("ALSA SoC WM1811");
MODULE_LICENSE("GPL");
