/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 * Seung-Woo Kim <sw0312.kim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * Based on drivers/media/video/s5p-tv/hdmi_drv.c
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include "drmP.h"
#include "drm_edid.h"
#include "drm_crtc_helper.h"

#include "regs-hdmi.h"

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_EXTCON
#include <linux/extcon.h>
#endif
#ifdef CONFIG_JACK_MON
#include <linux/jack.h>
#endif

#include <drm/exynos_drm.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_hdmi.h"
#include "exynos_drm_iommu.h"

#include "exynos_hdmi.h"
#include "exynos_hdcp.h"

#define MAX_WIDTH		1920
#define MAX_HEIGHT		1080
#define get_hdmi_context(dev)	platform_get_drvdata(to_platform_device(dev))

struct hdmi_resources {
	struct clk			*hdmi;
	struct clk			*sclk_hdmi;
	struct clk			*sclk_pixel;
	struct clk			*sclk_hdmiphy;
	struct clk			*hdmiphy;
	struct regulator_bulk_data	*regul_bulk;
	int				regul_count;
};

struct hdmi_context {
	struct device			*dev;
	struct drm_device		*drm_dev;
	atomic_t			hpd;
	bool				powered;
	bool				is_v13;
	bool				dvi_mode;
	bool				iommu_on;
	struct drm_exynos_hdmi_audio audio;
	struct mutex			hdmi_mutex;

	struct resource			*regs_res;
	void __iomem			*regs;
	void __iomem			*saved_regs;
	unsigned int			external_irq;
	unsigned int			internal_irq;

	struct i2c_client		*ddc_port;
	struct i2c_client		*hdmiphy_port;

	/* current hdmiphy conf index */
	int cur_conf;

	struct exynos_drm_hdmi_context	*hdcp_ctx;
	struct exynos_drm_hdmi_context	*cec_ctx;

#ifdef CONFIG_EXTCON
	struct extcon_dev	*extcon;
	const char *extcon_name;
#endif

	struct hdmi_resources		res;
	void				*parent_ctx;

	void				(*cfg_hpd)(bool external);
	int				(*get_hpd)(void);
};

/* HDMI Version 1.3 */
static const u8 hdmiphy_v13_conf27[32] = {
	0x01, 0x05, 0x00, 0xD8, 0x10, 0x1C, 0x30, 0x40,
	0x6B, 0x10, 0x02, 0x51, 0xDF, 0xF2, 0x54, 0x87,
	0x84, 0x00, 0x30, 0x38, 0x00, 0x08, 0x10, 0xE0,
	0x22, 0x40, 0xE3, 0x26, 0x00, 0x00, 0x00, 0x00,
};

static const u8 hdmiphy_v13_conf27_027[32] = {
	0x01, 0x05, 0x00, 0xD4, 0x10, 0x9C, 0x09, 0x64,
	0x6B, 0x10, 0x02, 0x51, 0xDF, 0xF2, 0x54, 0x87,
	0x84, 0x00, 0x30, 0x38, 0x00, 0x08, 0x10, 0xE0,
	0x22, 0x40, 0xE3, 0x26, 0x00, 0x00, 0x00, 0x00,
};

static const u8 hdmiphy_v13_conf74_175[32] = {
	0x01, 0x05, 0x00, 0xD8, 0x10, 0x9C, 0xef, 0x5B,
	0x6D, 0x10, 0x01, 0x51, 0xef, 0xF3, 0x54, 0xb9,
	0x84, 0x00, 0x30, 0x38, 0x00, 0x08, 0x10, 0xE0,
	0x22, 0x40, 0xa5, 0x26, 0x01, 0x00, 0x00, 0x00,
};

static const u8 hdmiphy_v13_conf74_25[32] = {
	0x01, 0x05, 0x00, 0xd8, 0x10, 0x9c, 0xf8, 0x40,
	0x6a, 0x10, 0x01, 0x51, 0xff, 0xf1, 0x54, 0xba,
	0x84, 0x00, 0x10, 0x38, 0x00, 0x08, 0x10, 0xe0,
	0x22, 0x40, 0xa4, 0x26, 0x01, 0x00, 0x00, 0x00,
};

static const u8 hdmiphy_v13_conf148_5[32] = {
	0x01, 0x05, 0x00, 0xD8, 0x10, 0x9C, 0xf8, 0x40,
	0x6A, 0x18, 0x00, 0x51, 0xff, 0xF1, 0x54, 0xba,
	0x84, 0x00, 0x10, 0x38, 0x00, 0x08, 0x10, 0xE0,
	0x22, 0x40, 0xa4, 0x26, 0x02, 0x00, 0x00, 0x00,
};

struct hdmi_v13_tg_regs {
	u8 cmd;
	u8 h_fsz_l;
	u8 h_fsz_h;
	u8 hact_st_l;
	u8 hact_st_h;
	u8 hact_sz_l;
	u8 hact_sz_h;
	u8 v_fsz_l;
	u8 v_fsz_h;
	u8 vsync_l;
	u8 vsync_h;
	u8 vsync2_l;
	u8 vsync2_h;
	u8 vact_st_l;
	u8 vact_st_h;
	u8 vact_sz_l;
	u8 vact_sz_h;
	u8 field_chg_l;
	u8 field_chg_h;
	u8 vact_st2_l;
	u8 vact_st2_h;
	u8 vsync_top_hdmi_l;
	u8 vsync_top_hdmi_h;
	u8 vsync_bot_hdmi_l;
	u8 vsync_bot_hdmi_h;
	u8 field_top_hdmi_l;
	u8 field_top_hdmi_h;
	u8 field_bot_hdmi_l;
	u8 field_bot_hdmi_h;
};

struct hdmi_v13_core_regs {
	u8 h_blank[2];
	u8 v_blank[3];
	u8 h_v_line[3];
	u8 vsync_pol[1];
	u8 int_pro_mode[1];
	u8 v_blank_f[3];
	u8 h_sync_gen[3];
	u8 v_sync_gen1[3];
	u8 v_sync_gen2[3];
	u8 v_sync_gen3[3];
};

struct hdmi_v13_preset_conf {
	struct hdmi_v13_core_regs core;
	struct hdmi_v13_tg_regs tg;
};

struct hdmi_v13_conf {
	int width;
	int height;
	int vrefresh;
	bool interlace;
	const u8 *hdmiphy_data;
	const struct hdmi_v13_preset_conf *conf;
};

static const struct hdmi_v13_preset_conf hdmi_v13_conf_480p = {
	.core = {
		.h_blank = {0x8a, 0x00},
		.v_blank = {0x0d, 0x6a, 0x01},
		.h_v_line = {0x0d, 0xa2, 0x35},
		.vsync_pol = {0x01},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00},
		.h_sync_gen = {0x0e, 0x30, 0x11},
		.v_sync_gen1 = {0x0f, 0x90, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x5a, 0x03, /* h_fsz */
		0x8a, 0x00, 0xd0, 0x02, /* hact */
		0x0d, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0xe0, 0x01, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_preset_conf hdmi_v13_conf_720p60 = {
	.core = {
		.h_blank = {0x72, 0x01},
		.v_blank = {0xee, 0xf2, 0x00},
		.h_v_line = {0xee, 0x22, 0x67},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00}, /* don't care */
		.h_sync_gen = {0x6c, 0x50, 0x02},
		.v_sync_gen1 = {0x0a, 0x50, 0x00},
		.v_sync_gen2 = {0x01, 0x10, 0x00},
		.v_sync_gen3 = {0x01, 0x10, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x72, 0x06, /* h_fsz */
		0x72, 0x01, 0x00, 0x05, /* hact */
		0xee, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x1e, 0x00, 0xd0, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_preset_conf hdmi_v13_conf_1080p30 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v_blank = {0x65, 0x6c, 0x01},
		.h_v_line = {0x65, 0x84, 0x89},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00}, /* don't care */
		.h_sync_gen = {0x56, 0x08, 0x02},
		.v_sync_gen1 = {0x09, 0x40, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_preset_conf hdmi_v13_conf_1080i50 = {
	.core = {
		.h_blank = {0xd0, 0x02},
		.v_blank = {0x32, 0xB2, 0x00},
		.h_v_line = {0x65, 0x04, 0xa5},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x01},
		.v_blank_f = {0x49, 0x2A, 0x23},
		.h_sync_gen = {0x0E, 0xEA, 0x08},
		.v_sync_gen1 = {0x07, 0x20, 0x00},
		.v_sync_gen2 = {0x39, 0x42, 0x23},
		.v_sync_gen3 = {0x38, 0x87, 0x73},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x50, 0x0A, /* h_fsz */
		0xCF, 0x02, 0x81, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x16, 0x00, 0x1c, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_preset_conf hdmi_v13_conf_1080p50 = {
	.core = {
		.h_blank = {0xd0, 0x02},
		.v_blank = {0x65, 0x6c, 0x01},
		.h_v_line = {0x65, 0x04, 0xa5},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00}, /* don't care */
		.h_sync_gen = {0x0e, 0xea, 0x08},
		.v_sync_gen1 = {0x09, 0x40, 0x00},
		.v_sync_gen2 = {0x01, 0x10, 0x00},
		.v_sync_gen3 = {0x01, 0x10, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_preset_conf hdmi_v13_conf_1080i60 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v_blank = {0x32, 0xb2, 0x00},
		.h_v_line = {0x65, 0x84, 0x89},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x01},
		.v_blank_f = {0x49, 0x2a, 0x23},
		.h_sync_gen = {0x56, 0x08, 0x02},
		.v_sync_gen1 = {0x07, 0x20, 0x00},
		.v_sync_gen2 = {0x39, 0x42, 0x23},
		.v_sync_gen3 = {0xa4, 0x44, 0x4a},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x17, 0x01, 0x81, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x16, 0x00, 0x1c, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_preset_conf hdmi_v13_conf_1080p60 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v_blank = {0x65, 0x6c, 0x01},
		.h_v_line = {0x65, 0x84, 0x89},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f = {0x00, 0x00, 0x00}, /* don't care */
		.h_sync_gen = {0x56, 0x08, 0x02},
		.v_sync_gen1 = {0x09, 0x40, 0x00},
		.v_sync_gen2 = {0x01, 0x10, 0x00},
		.v_sync_gen3 = {0x01, 0x10, 0x00},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
	},
};

static const struct hdmi_v13_conf hdmi_v13_confs[] = {
	{ 720, 480, 60, false, hdmiphy_v13_conf27_027, &hdmi_v13_conf_480p },
	{ 1280, 720, 50, false, hdmiphy_v13_conf74_25, &hdmi_v13_conf_720p60 },
	{ 1280, 720, 60, false, hdmiphy_v13_conf74_25, &hdmi_v13_conf_720p60 },
	{ 1920, 1080, 30, false, hdmiphy_v13_conf148_5,
		&hdmi_v13_conf_1080p30 },
};

/* HDMI Version 1.4 */
static const u8 hdmiphy_conf27_027[32] = {
	0x01, 0xd1, 0x2d, 0x72, 0x40, 0x64, 0x12, 0x08,
	0x43, 0xa0, 0x0e, 0xd9, 0x45, 0xa0, 0xac, 0x80,
	0x08, 0x80, 0x11, 0x04, 0x02, 0x22, 0x44, 0x86,
	0x54, 0xe3, 0x24, 0x00, 0x00, 0x00, 0x01, 0x00,
};

static const u8 hdmiphy_conf74_176[32] = {
	0x01, 0xd1, 0x1f, 0x10, 0x40, 0x5b, 0xef, 0x08,
	0x81, 0xa0, 0xb9, 0xd8, 0x45, 0xa0, 0xac, 0x80,
	0x5a, 0x80, 0x11, 0x04, 0x02, 0x22, 0x44, 0x86,
	0x54, 0xa6, 0x24, 0x01, 0x00, 0x00, 0x01, 0x00,
};

static const u8 hdmiphy_conf74_25[32] = {
	0x01, 0xd1, 0x1f, 0x10, 0x40, 0x40, 0xf8, 0x08,
	0x81, 0xa0, 0xba, 0xd8, 0x45, 0xa0, 0xac, 0x80,
	0x3c, 0x80, 0x11, 0x04, 0x02, 0x22, 0x44, 0x86,
	0x54, 0xa5, 0x24, 0x01, 0x00, 0x00, 0x01, 0x00,
};

static const u8 hdmiphy_conf148_5[32] = {
	0x01, 0xd1, 0x1f, 0x00, 0x40, 0x40, 0xf8, 0x08,
	0x81, 0xa0, 0xba, 0xd8, 0x45, 0xa0, 0xac, 0x80,
	0x3c, 0x80, 0x11, 0x04, 0x02, 0x22, 0x44, 0x86,
	0x54, 0x4b, 0x25, 0x03, 0x00, 0x00, 0x01, 0x00,
};

struct hdmi_tg_regs {
	u8 cmd;
	u8 h_fsz_l;
	u8 h_fsz_h;
	u8 hact_st_l;
	u8 hact_st_h;
	u8 hact_sz_l;
	u8 hact_sz_h;
	u8 v_fsz_l;
	u8 v_fsz_h;
	u8 vsync_l;
	u8 vsync_h;
	u8 vsync2_l;
	u8 vsync2_h;
	u8 vact_st_l;
	u8 vact_st_h;
	u8 vact_sz_l;
	u8 vact_sz_h;
	u8 field_chg_l;
	u8 field_chg_h;
	u8 vact_st2_l;
	u8 vact_st2_h;
	u8 vact_st3_l;
	u8 vact_st3_h;
	u8 vact_st4_l;
	u8 vact_st4_h;
	u8 vsync_top_hdmi_l;
	u8 vsync_top_hdmi_h;
	u8 vsync_bot_hdmi_l;
	u8 vsync_bot_hdmi_h;
	u8 field_top_hdmi_l;
	u8 field_top_hdmi_h;
	u8 field_bot_hdmi_l;
	u8 field_bot_hdmi_h;
	u8 tg_3d;
};

struct hdmi_core_regs {
	u8 h_blank[2];
	u8 v2_blank[2];
	u8 v1_blank[2];
	u8 v_line[2];
	u8 h_line[2];
	u8 hsync_pol[1];
	u8 vsync_pol[1];
	u8 int_pro_mode[1];
	u8 v_blank_f0[2];
	u8 v_blank_f1[2];
	u8 h_sync_start[2];
	u8 h_sync_end[2];
	u8 v_sync_line_bef_2[2];
	u8 v_sync_line_bef_1[2];
	u8 v_sync_line_aft_2[2];
	u8 v_sync_line_aft_1[2];
	u8 v_sync_line_aft_pxl_2[2];
	u8 v_sync_line_aft_pxl_1[2];
	u8 v_blank_f2[2]; /* for 3D mode */
	u8 v_blank_f3[2]; /* for 3D mode */
	u8 v_blank_f4[2]; /* for 3D mode */
	u8 v_blank_f5[2]; /* for 3D mode */
	u8 v_sync_line_aft_3[2];
	u8 v_sync_line_aft_4[2];
	u8 v_sync_line_aft_5[2];
	u8 v_sync_line_aft_6[2];
	u8 v_sync_line_aft_pxl_3[2];
	u8 v_sync_line_aft_pxl_4[2];
	u8 v_sync_line_aft_pxl_5[2];
	u8 v_sync_line_aft_pxl_6[2];
	u8 vact_space_1[2];
	u8 vact_space_2[2];
	u8 vact_space_3[2];
	u8 vact_space_4[2];
	u8 vact_space_5[2];
	u8 vact_space_6[2];
};

struct hdmi_preset_conf {
	struct hdmi_core_regs core;
	struct hdmi_tg_regs tg;
};

struct hdmi_conf {
	int width;
	int height;
	int vrefresh;
	bool interlace;
	const u8 *hdmiphy_data;
	const struct hdmi_preset_conf *conf;
};

static const struct hdmi_preset_conf hdmi_conf_480p60 = {
	.core = {
		.h_blank = {0x8a, 0x00},
		.v2_blank = {0x0d, 0x02},
		.v1_blank = {0x2d, 0x00},
		.v_line = {0x0d, 0x02},
		.h_line = {0x5a, 0x03},
		.hsync_pol = {0x01},
		.vsync_pol = {0x01},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x0e, 0x00},
		.h_sync_end = {0x4c, 0x00},
		.v_sync_line_bef_2 = {0x0f, 0x00},
		.v_sync_line_bef_1 = {0x09, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x5a, 0x03, /* h_fsz */
		0x8a, 0x00, 0xd0, 0x02, /* hact */
		0x0d, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0xe0, 0x01, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_preset_conf hdmi_conf_720p50 = {
	.core = {
		.h_blank = {0xbc, 0x02},
		.v2_blank = {0xee, 0x02},
		.v1_blank = {0x1e, 0x00},
		.v_line = {0xee, 0x02},
		.h_line = {0xbc, 0x07},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0xb6, 0x01},
		.h_sync_end = {0xde, 0x01},
		.v_sync_line_bef_2 = {0x0a, 0x00},
		.v_sync_line_bef_1 = {0x05, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0xbc, 0x07, /* h_fsz */
		0xbc, 0x02, 0x00, 0x05, /* hact */
		0xee, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x1e, 0x00, 0xd0, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_preset_conf hdmi_conf_720p60 = {
	.core = {
		.h_blank = {0x72, 0x01},
		.v2_blank = {0xee, 0x02},
		.v1_blank = {0x1e, 0x00},
		.v_line = {0xee, 0x02},
		.h_line = {0x72, 0x06},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x6c, 0x00},
		.h_sync_end = {0x94, 0x00},
		.v_sync_line_bef_2 = {0x0a, 0x00},
		.v_sync_line_bef_1 = {0x05, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x72, 0x06, /* h_fsz */
		0x72, 0x01, 0x00, 0x05, /* hact */
		0xee, 0x02, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x1e, 0x00, 0xd0, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_preset_conf hdmi_conf_1080i50 = {
	.core = {
		.h_blank = {0xd0, 0x02},
		.v2_blank = {0x32, 0x02},
		.v1_blank = {0x16, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x50, 0x0a},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x01},
		.v_blank_f0 = {0x49, 0x02},
		.v_blank_f1 = {0x65, 0x04},
		.h_sync_start = {0x0e, 0x02},
		.h_sync_end = {0x3a, 0x02},
		.v_sync_line_bef_2 = {0x07, 0x00},
		.v_sync_line_bef_1 = {0x02, 0x00},
		.v_sync_line_aft_2 = {0x39, 0x02},
		.v_sync_line_aft_1 = {0x34, 0x02},
		.v_sync_line_aft_pxl_2 = {0x38, 0x07},
		.v_sync_line_aft_pxl_1 = {0x38, 0x07},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x50, 0x0a, /* h_fsz */
		0xd0, 0x02, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x16, 0x00, 0x1c, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_preset_conf hdmi_conf_1080i60 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v2_blank = {0x32, 0x02},
		.v1_blank = {0x16, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x98, 0x08},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x01},
		.v_blank_f0 = {0x49, 0x02},
		.v_blank_f1 = {0x65, 0x04},
		.h_sync_start = {0x56, 0x00},
		.h_sync_end = {0x82, 0x00},
		.v_sync_line_bef_2 = {0x07, 0x00},
		.v_sync_line_bef_1 = {0x02, 0x00},
		.v_sync_line_aft_2 = {0x39, 0x02},
		.v_sync_line_aft_1 = {0x34, 0x02},
		.v_sync_line_aft_pxl_2 = {0xa4, 0x04},
		.v_sync_line_aft_pxl_1 = {0xa4, 0x04},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x16, 0x00, 0x1c, 0x02, /* vact */
		0x33, 0x02, /* field_chg */
		0x49, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x33, 0x02, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_preset_conf hdmi_conf_1080p30 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v2_blank = {0x65, 0x04},
		.v1_blank = {0x2d, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x98, 0x08},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x56, 0x00},
		.h_sync_end = {0x82, 0x00},
		.v_sync_line_bef_2 = {0x09, 0x00},
		.v_sync_line_bef_1 = {0x04, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_preset_conf hdmi_conf_1080p50 = {
	.core = {
		.h_blank = {0xd0, 0x02},
		.v2_blank = {0x65, 0x04},
		.v1_blank = {0x2d, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x50, 0x0a},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x0e, 0x02},
		.h_sync_end = {0x3a, 0x02},
		.v_sync_line_bef_2 = {0x09, 0x00},
		.v_sync_line_bef_1 = {0x04, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		.vact_space_1 = {0xff, 0xff},
		.vact_space_2 = {0xff, 0xff},
		.vact_space_3 = {0xff, 0xff},
		.vact_space_4 = {0xff, 0xff},
		.vact_space_5 = {0xff, 0xff},
		.vact_space_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x50, 0x0a, /* h_fsz */
		0xd0, 0x02, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_preset_conf hdmi_conf_1080p60 = {
	.core = {
		.h_blank = {0x18, 0x01},
		.v2_blank = {0x65, 0x04},
		.v1_blank = {0x2d, 0x00},
		.v_line = {0x65, 0x04},
		.h_line = {0x98, 0x08},
		.hsync_pol = {0x00},
		.vsync_pol = {0x00},
		.int_pro_mode = {0x00},
		.v_blank_f0 = {0xff, 0xff},
		.v_blank_f1 = {0xff, 0xff},
		.h_sync_start = {0x56, 0x00},
		.h_sync_end = {0x82, 0x00},
		.v_sync_line_bef_2 = {0x09, 0x00},
		.v_sync_line_bef_1 = {0x04, 0x00},
		.v_sync_line_aft_2 = {0xff, 0xff},
		.v_sync_line_aft_1 = {0xff, 0xff},
		.v_sync_line_aft_pxl_2 = {0xff, 0xff},
		.v_sync_line_aft_pxl_1 = {0xff, 0xff},
		.v_blank_f2 = {0xff, 0xff},
		.v_blank_f3 = {0xff, 0xff},
		.v_blank_f4 = {0xff, 0xff},
		.v_blank_f5 = {0xff, 0xff},
		.v_sync_line_aft_3 = {0xff, 0xff},
		.v_sync_line_aft_4 = {0xff, 0xff},
		.v_sync_line_aft_5 = {0xff, 0xff},
		.v_sync_line_aft_6 = {0xff, 0xff},
		.v_sync_line_aft_pxl_3 = {0xff, 0xff},
		.v_sync_line_aft_pxl_4 = {0xff, 0xff},
		.v_sync_line_aft_pxl_5 = {0xff, 0xff},
		.v_sync_line_aft_pxl_6 = {0xff, 0xff},
		/* other don't care */
	},
	.tg = {
		0x00, /* cmd */
		0x98, 0x08, /* h_fsz */
		0x18, 0x01, 0x80, 0x07, /* hact */
		0x65, 0x04, /* v_fsz */
		0x01, 0x00, 0x33, 0x02, /* vsync */
		0x2d, 0x00, 0x38, 0x04, /* vact */
		0x33, 0x02, /* field_chg */
		0x48, 0x02, /* vact_st2 */
		0x00, 0x00, /* vact_st3 */
		0x00, 0x00, /* vact_st4 */
		0x01, 0x00, 0x01, 0x00, /* vsync top/bot */
		0x01, 0x00, 0x33, 0x02, /* field top/bot */
		0x00, /* 3d FP */
	},
};

static const struct hdmi_conf hdmi_confs[] = {
	{ 720, 480, 60, false, hdmiphy_conf27_027, &hdmi_conf_480p60 },
	{ 1280, 720, 50, false, hdmiphy_conf74_25, &hdmi_conf_720p50 },
	{ 1280, 720, 60, false, hdmiphy_conf74_25, &hdmi_conf_720p60 },
	{ 1920, 1080, 30, false, hdmiphy_conf74_176, &hdmi_conf_1080p30 },
};

#ifdef CONFIG_EXTCON
static const char fake_edid_info[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x4C, 0x2D, 0xCA, 0x07,
	0x4A, 0x44, 0x46, 0x59, 0x25, 0x15, 0x01, 0x03, 0x80, 0x33, 0x1D, 0x78,
	0x2A, 0x01, 0xF1, 0xA2, 0x57, 0x52, 0x9F, 0x27, 0x0A, 0x50, 0x54, 0xBF,
	0xEF, 0x80, 0x71, 0x4F, 0x81, 0x00, 0x81, 0x40, 0x81, 0x80, 0x95, 0x00,
	0x95, 0x0F, 0xA9, 0x40, 0xB3, 0x00, 0x02, 0x3A, 0x80, 0x18, 0x71, 0x38,
	0x2D, 0x40, 0x58, 0x2C, 0x45, 0x00, 0xFD, 0x1E, 0x11, 0x00, 0x00, 0x1E,
	0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28, 0x55, 0x00,
	0xFD, 0x1E, 0x11, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x32,
	0x4B, 0x1F, 0x51, 0x11, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0xFC, 0x00, 0x53, 0x4D, 0x53, 0x32, 0x33, 0x41, 0x35,
	0x35, 0x30, 0x48, 0x0A, 0x20, 0x20, 0x01, 0x80, 0x02, 0x03, 0x13, 0xB1,
	0x48, 0x90, 0x04, 0x1F, 0x05, 0x14, 0x13, 0x12, 0x03, 0x65, 0x03, 0x0C,
	0x00, 0x10, 0x00, 0x01, 0x1D, 0x80, 0xD0, 0x72, 0x1C, 0x16, 0x20, 0x10,
	0x2C, 0x25, 0x80, 0xFD, 0x1E, 0x11, 0x00, 0x00, 0x9E, 0x01, 0x1D, 0x80,
	0x18, 0x71, 0x1C, 0x16, 0x20, 0x58, 0x2C, 0x25, 0x00, 0xFD, 0x1E, 0x11,
	0x00, 0x00, 0x9E, 0x01, 0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20, 0xB8,
	0x28, 0x55, 0x40, 0xFD, 0x1E, 0x11, 0x00, 0x00, 0x1E, 0x8C, 0x0A, 0xD0,
	0x90, 0x20, 0x40, 0x31, 0x20, 0x0C, 0x40, 0x55, 0x00, 0xFD, 0x1E, 0x11,
	0x00, 0x00, 0x18, 0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10,
	0x3E, 0x96, 0x00, 0xFD, 0x1E, 0x11, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xD4
};
#endif

static inline u32 hdmi_reg_read(struct hdmi_context *hdata, u32 reg_id)
{
	return readl(hdata->regs + reg_id);
}

static inline void hdmi_reg_writeb(struct hdmi_context *hdata,
				 u32 reg_id, u8 value)
{
	writeb(value, hdata->regs + reg_id);
}

static inline void hdmi_reg_writemask(struct hdmi_context *hdata,
				 u32 reg_id, u32 value, u32 mask)
{
	u32 old = readl(hdata->regs + reg_id);
	value = (value & mask) | (old & ~mask);
	writel(value, hdata->regs + reg_id);
}

static void hdmi_v13_regs_dump(struct hdmi_context *hdata, char *prefix)
{
#define DUMPREG(reg_id) \
	DRM_DEBUG_KMS("%s:" #reg_id " = %08x\n", prefix, \
	readl(hdata->regs + reg_id))
	DRM_DEBUG_KMS("%s: ---- CONTROL REGISTERS ----\n", prefix);
	DUMPREG(HDMI_INTC_FLAG);
	DUMPREG(HDMI_INTC_CON);
	DUMPREG(HDMI_HPD_STATUS);
	DUMPREG(HDMI_V13_PHY_RSTOUT);
	DUMPREG(HDMI_V13_PHY_VPLL);
	DUMPREG(HDMI_V13_PHY_CMU);
	DUMPREG(HDMI_V13_CORE_RSTOUT);

	DRM_DEBUG_KMS("%s: ---- CORE REGISTERS ----\n", prefix);
	DUMPREG(HDMI_CON_0);
	DUMPREG(HDMI_CON_1);
	DUMPREG(HDMI_CON_2);
	DUMPREG(HDMI_SYS_STATUS);
	DUMPREG(HDMI_V13_PHY_STATUS);
	DUMPREG(HDMI_STATUS_EN);
	DUMPREG(HDMI_HPD);
	DUMPREG(HDMI_MODE_SEL);
	DUMPREG(HDMI_V13_HPD_GEN);
	DUMPREG(HDMI_V13_DC_CONTROL);
	DUMPREG(HDMI_V13_VIDEO_PATTERN_GEN);

	DRM_DEBUG_KMS("%s: ---- CORE SYNC REGISTERS ----\n", prefix);
	DUMPREG(HDMI_H_BLANK_0);
	DUMPREG(HDMI_H_BLANK_1);
	DUMPREG(HDMI_V13_V_BLANK_0);
	DUMPREG(HDMI_V13_V_BLANK_1);
	DUMPREG(HDMI_V13_V_BLANK_2);
	DUMPREG(HDMI_V13_H_V_LINE_0);
	DUMPREG(HDMI_V13_H_V_LINE_1);
	DUMPREG(HDMI_V13_H_V_LINE_2);
	DUMPREG(HDMI_VSYNC_POL);
	DUMPREG(HDMI_INT_PRO_MODE);
	DUMPREG(HDMI_V13_V_BLANK_F_0);
	DUMPREG(HDMI_V13_V_BLANK_F_1);
	DUMPREG(HDMI_V13_V_BLANK_F_2);
	DUMPREG(HDMI_V13_H_SYNC_GEN_0);
	DUMPREG(HDMI_V13_H_SYNC_GEN_1);
	DUMPREG(HDMI_V13_H_SYNC_GEN_2);
	DUMPREG(HDMI_V13_V_SYNC_GEN_1_0);
	DUMPREG(HDMI_V13_V_SYNC_GEN_1_1);
	DUMPREG(HDMI_V13_V_SYNC_GEN_1_2);
	DUMPREG(HDMI_V13_V_SYNC_GEN_2_0);
	DUMPREG(HDMI_V13_V_SYNC_GEN_2_1);
	DUMPREG(HDMI_V13_V_SYNC_GEN_2_2);
	DUMPREG(HDMI_V13_V_SYNC_GEN_3_0);
	DUMPREG(HDMI_V13_V_SYNC_GEN_3_1);
	DUMPREG(HDMI_V13_V_SYNC_GEN_3_2);

	DRM_DEBUG_KMS("%s: ---- TG REGISTERS ----\n", prefix);
	DUMPREG(HDMI_TG_CMD);
	DUMPREG(HDMI_TG_H_FSZ_L);
	DUMPREG(HDMI_TG_H_FSZ_H);
	DUMPREG(HDMI_TG_HACT_ST_L);
	DUMPREG(HDMI_TG_HACT_ST_H);
	DUMPREG(HDMI_TG_HACT_SZ_L);
	DUMPREG(HDMI_TG_HACT_SZ_H);
	DUMPREG(HDMI_TG_V_FSZ_L);
	DUMPREG(HDMI_TG_V_FSZ_H);
	DUMPREG(HDMI_TG_VSYNC_L);
	DUMPREG(HDMI_TG_VSYNC_H);
	DUMPREG(HDMI_TG_VSYNC2_L);
	DUMPREG(HDMI_TG_VSYNC2_H);
	DUMPREG(HDMI_TG_VACT_ST_L);
	DUMPREG(HDMI_TG_VACT_ST_H);
	DUMPREG(HDMI_TG_VACT_SZ_L);
	DUMPREG(HDMI_TG_VACT_SZ_H);
	DUMPREG(HDMI_TG_FIELD_CHG_L);
	DUMPREG(HDMI_TG_FIELD_CHG_H);
	DUMPREG(HDMI_TG_VACT_ST2_L);
	DUMPREG(HDMI_TG_VACT_ST2_H);
	DUMPREG(HDMI_TG_VSYNC_TOP_HDMI_L);
	DUMPREG(HDMI_TG_VSYNC_TOP_HDMI_H);
	DUMPREG(HDMI_TG_VSYNC_BOT_HDMI_L);
	DUMPREG(HDMI_TG_VSYNC_BOT_HDMI_H);
	DUMPREG(HDMI_TG_FIELD_TOP_HDMI_L);
	DUMPREG(HDMI_TG_FIELD_TOP_HDMI_H);
	DUMPREG(HDMI_TG_FIELD_BOT_HDMI_L);
	DUMPREG(HDMI_TG_FIELD_BOT_HDMI_H);
#undef DUMPREG
}

static void hdmi_v14_regs_dump(struct hdmi_context *hdata, char *prefix)
{
	int i;

#define DUMPREG(reg_id) \
	DRM_DEBUG_KMS("%s:" #reg_id " = %08x\n", prefix, \
	readl(hdata->regs + reg_id))

	DRM_DEBUG_KMS("%s: ---- CONTROL REGISTERS ----\n", prefix);
	DUMPREG(HDMI_INTC_CON);
	DUMPREG(HDMI_INTC_FLAG);
	DUMPREG(HDMI_HPD_STATUS);
	DUMPREG(HDMI_INTC_CON_1);
	DUMPREG(HDMI_INTC_FLAG_1);
	DUMPREG(HDMI_PHY_STATUS_0);
	DUMPREG(HDMI_PHY_STATUS_PLL);
	DUMPREG(HDMI_PHY_CON_0);
	DUMPREG(HDMI_PHY_RSTOUT);
	DUMPREG(HDMI_PHY_VPLL);
	DUMPREG(HDMI_PHY_CMU);
	DUMPREG(HDMI_CORE_RSTOUT);

	DRM_DEBUG_KMS("%s: ---- CORE REGISTERS ----\n", prefix);
	DUMPREG(HDMI_CON_0);
	DUMPREG(HDMI_CON_1);
	DUMPREG(HDMI_CON_2);
	DUMPREG(HDMI_SYS_STATUS);
	DUMPREG(HDMI_PHY_STATUS_0);
	DUMPREG(HDMI_STATUS_EN);
	DUMPREG(HDMI_HPD);
	DUMPREG(HDMI_MODE_SEL);
	DUMPREG(HDMI_ENC_EN);
	DUMPREG(HDMI_DC_CONTROL);
	DUMPREG(HDMI_VIDEO_PATTERN_GEN);

	DRM_DEBUG_KMS("%s: ---- CORE SYNC REGISTERS ----\n", prefix);
	DUMPREG(HDMI_H_BLANK_0);
	DUMPREG(HDMI_H_BLANK_1);
	DUMPREG(HDMI_V2_BLANK_0);
	DUMPREG(HDMI_V2_BLANK_1);
	DUMPREG(HDMI_V1_BLANK_0);
	DUMPREG(HDMI_V1_BLANK_1);
	DUMPREG(HDMI_V_LINE_0);
	DUMPREG(HDMI_V_LINE_1);
	DUMPREG(HDMI_H_LINE_0);
	DUMPREG(HDMI_H_LINE_1);
	DUMPREG(HDMI_HSYNC_POL);

	DUMPREG(HDMI_VSYNC_POL);
	DUMPREG(HDMI_INT_PRO_MODE);
	DUMPREG(HDMI_V_BLANK_F0_0);
	DUMPREG(HDMI_V_BLANK_F0_1);
	DUMPREG(HDMI_V_BLANK_F1_0);
	DUMPREG(HDMI_V_BLANK_F1_1);

	DUMPREG(HDMI_H_SYNC_START_0);
	DUMPREG(HDMI_H_SYNC_START_1);
	DUMPREG(HDMI_H_SYNC_END_0);
	DUMPREG(HDMI_H_SYNC_END_1);

	DUMPREG(HDMI_V_SYNC_LINE_BEF_2_0);
	DUMPREG(HDMI_V_SYNC_LINE_BEF_2_1);
	DUMPREG(HDMI_V_SYNC_LINE_BEF_1_0);
	DUMPREG(HDMI_V_SYNC_LINE_BEF_1_1);

	DUMPREG(HDMI_V_SYNC_LINE_AFT_2_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_2_1);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_1_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_1_1);

	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_2_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_2_1);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_1_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_1_1);

	DUMPREG(HDMI_V_BLANK_F2_0);
	DUMPREG(HDMI_V_BLANK_F2_1);
	DUMPREG(HDMI_V_BLANK_F3_0);
	DUMPREG(HDMI_V_BLANK_F3_1);
	DUMPREG(HDMI_V_BLANK_F4_0);
	DUMPREG(HDMI_V_BLANK_F4_1);
	DUMPREG(HDMI_V_BLANK_F5_0);
	DUMPREG(HDMI_V_BLANK_F5_1);

	DUMPREG(HDMI_V_SYNC_LINE_AFT_3_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_3_1);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_4_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_4_1);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_5_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_5_1);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_6_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_6_1);

	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_3_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_3_1);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_4_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_4_1);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_5_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_5_1);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_6_0);
	DUMPREG(HDMI_V_SYNC_LINE_AFT_PXL_6_1);

	DUMPREG(HDMI_VACT_SPACE_1_0);
	DUMPREG(HDMI_VACT_SPACE_1_1);
	DUMPREG(HDMI_VACT_SPACE_2_0);
	DUMPREG(HDMI_VACT_SPACE_2_1);
	DUMPREG(HDMI_VACT_SPACE_3_0);
	DUMPREG(HDMI_VACT_SPACE_3_1);
	DUMPREG(HDMI_VACT_SPACE_4_0);
	DUMPREG(HDMI_VACT_SPACE_4_1);
	DUMPREG(HDMI_VACT_SPACE_5_0);
	DUMPREG(HDMI_VACT_SPACE_5_1);
	DUMPREG(HDMI_VACT_SPACE_6_0);
	DUMPREG(HDMI_VACT_SPACE_6_1);

	DRM_DEBUG_KMS("%s: ---- TG REGISTERS ----\n", prefix);
	DUMPREG(HDMI_TG_CMD);
	DUMPREG(HDMI_TG_H_FSZ_L);
	DUMPREG(HDMI_TG_H_FSZ_H);
	DUMPREG(HDMI_TG_HACT_ST_L);
	DUMPREG(HDMI_TG_HACT_ST_H);
	DUMPREG(HDMI_TG_HACT_SZ_L);
	DUMPREG(HDMI_TG_HACT_SZ_H);
	DUMPREG(HDMI_TG_V_FSZ_L);
	DUMPREG(HDMI_TG_V_FSZ_H);
	DUMPREG(HDMI_TG_VSYNC_L);
	DUMPREG(HDMI_TG_VSYNC_H);
	DUMPREG(HDMI_TG_VSYNC2_L);
	DUMPREG(HDMI_TG_VSYNC2_H);
	DUMPREG(HDMI_TG_VACT_ST_L);
	DUMPREG(HDMI_TG_VACT_ST_H);
	DUMPREG(HDMI_TG_VACT_SZ_L);
	DUMPREG(HDMI_TG_VACT_SZ_H);
	DUMPREG(HDMI_TG_FIELD_CHG_L);
	DUMPREG(HDMI_TG_FIELD_CHG_H);
	DUMPREG(HDMI_TG_VACT_ST2_L);
	DUMPREG(HDMI_TG_VACT_ST2_H);
	DUMPREG(HDMI_TG_VACT_ST3_L);
	DUMPREG(HDMI_TG_VACT_ST3_H);
	DUMPREG(HDMI_TG_VACT_ST4_L);
	DUMPREG(HDMI_TG_VACT_ST4_H);
	DUMPREG(HDMI_TG_VSYNC_TOP_HDMI_L);
	DUMPREG(HDMI_TG_VSYNC_TOP_HDMI_H);
	DUMPREG(HDMI_TG_VSYNC_BOT_HDMI_L);
	DUMPREG(HDMI_TG_VSYNC_BOT_HDMI_H);
	DUMPREG(HDMI_TG_FIELD_TOP_HDMI_L);
	DUMPREG(HDMI_TG_FIELD_TOP_HDMI_H);
	DUMPREG(HDMI_TG_FIELD_BOT_HDMI_L);
	DUMPREG(HDMI_TG_FIELD_BOT_HDMI_H);
	DUMPREG(HDMI_TG_3D);

	DRM_DEBUG_KMS("%s: ---- PACKET REGISTERS ----\n", prefix);
	DUMPREG(HDMI_AVI_CON);
	DUMPREG(HDMI_AVI_HEADER0);
	DUMPREG(HDMI_AVI_HEADER1);
	DUMPREG(HDMI_AVI_HEADER2);
	DUMPREG(HDMI_AVI_CHECK_SUM);
	DUMPREG(HDMI_VSI_CON);
	DUMPREG(HDMI_VSI_HEADER0);
	DUMPREG(HDMI_VSI_HEADER1);
	DUMPREG(HDMI_VSI_HEADER2);
	for (i = 0; i < 7; ++i)
		DUMPREG(HDMI_VSI_DATA(i));

#undef DUMPREG
}

static struct exynos_hdcp_ops *hdcp_ops;
static struct exynos_cec_ops *cec_ops;

void exynos_hdcp_ops_register(struct exynos_hdcp_ops *ops)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (ops)
		hdcp_ops = ops;
}

void exynos_cec_ops_register(struct exynos_cec_ops *ops)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (ops)
		cec_ops = ops;
}

static void hdmi_regs_dump(struct hdmi_context *hdata, char *prefix)
{
	if (hdata->is_v13)
		hdmi_v13_regs_dump(hdata, prefix);
	else
		hdmi_v14_regs_dump(hdata, prefix);
}

static int hdmi_v13_conf_index(struct drm_display_mode *mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdmi_v13_confs); ++i)
		if (hdmi_v13_confs[i].width == mode->hdisplay &&
				hdmi_v13_confs[i].height == mode->vdisplay &&
				hdmi_v13_confs[i].vrefresh == mode->vrefresh &&
				hdmi_v13_confs[i].interlace ==
				((mode->flags & DRM_MODE_FLAG_INTERLACE) ?
				 true : false))
			return i;

	return -EINVAL;
}

static int hdmi_v14_conf_index(struct drm_display_mode *mode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdmi_confs); ++i)
		if (hdmi_confs[i].width == mode->hdisplay &&
				hdmi_confs[i].height == mode->vdisplay &&
				hdmi_confs[i].vrefresh == mode->vrefresh &&
				hdmi_confs[i].interlace ==
				((mode->flags & DRM_MODE_FLAG_INTERLACE) ?
				 true : false))
			return i;

	return -EINVAL;
}

static int hdmi_conf_index(struct hdmi_context *hdata,
			   struct drm_display_mode *mode)
{
	if (hdata->is_v13)
		return hdmi_v13_conf_index(mode);

	return hdmi_v14_conf_index(mode);
}

static bool hdmi_is_connected(void *ctx)
{
	struct hdmi_context *hdata = ctx;

	DRM_DEBUG_KMS("[%d] %s:hpd[%d]\n", __LINE__,
		__func__, atomic_read(&hdata->hpd));

#ifdef CONFIG_EXTCON
	if (hdata->extcon)
		return atomic_read(&hdata->hpd) &&
			extcon_get_cable_state(hdata->extcon, "MHL");
#endif

	return atomic_read(&hdata->hpd);
}

static int hdmi_get_edid(void *ctx, struct drm_connector *connector,
				u8 *edid, int len)
{
	struct edid *raw_edid;
	struct hdmi_context *hdata = ctx;

	DRM_DEBUG_KMS("[%d] %s:powered[%d]hpd[%d]\n", __LINE__,
		__func__, hdata->powered, atomic_read(&hdata->hpd));

	if (!hdata->ddc_port)
		return -ENODEV;

	raw_edid = drm_get_edid(connector, hdata->ddc_port->adapter);

#ifdef CONFIG_EXTCON
	if (hdata->extcon) {
		int fake_len;

		DRM_DEBUG_KMS("[%d] %s:MHL cable[%d]\n", __LINE__,
			__func__, extcon_get_cable_state(hdata->extcon, "MHL"));

		if (!extcon_get_cable_state(hdata->extcon, "MHL"))
			return -ENODEV;

		/* make fake edid for old devices */
		if (!raw_edid) {

			DRM_ERROR("failed to get raw_edid.\n");

			raw_edid = kzalloc(len, GFP_KERNEL);
			if (!raw_edid) {
				DRM_DEBUG_KMS("failed to allocate raw_edid.\n");
				return -ENOMEM;
			}

			fake_len = sizeof(fake_edid_info);
			memcpy(raw_edid, fake_edid_info, min(fake_len, len));

			/* attach the edid data to connector. */
			connector->display_info.raw_edid = (char *)raw_edid;
		}
	}
#endif

	if (raw_edid) {
		hdata->dvi_mode = !drm_detect_hdmi_monitor(raw_edid);
		memcpy(edid, raw_edid, min((1 + raw_edid->extensions)
					* EDID_LENGTH, len));
		DRM_DEBUG_KMS("%s : width[%d] x height[%d]\n",
			(hdata->dvi_mode ? "dvi monitor" : "hdmi monitor"),
			raw_edid->width_cm, raw_edid->height_cm);
	} else
		return -ENODEV;

	return 0;
}

static int hdmi_v13_check_timing(struct fb_videomode *check_timing)
{
	int i;

	DRM_DEBUG_KMS("valid mode : xres=%d, yres=%d, refresh=%d, intl=%d\n",
			check_timing->xres, check_timing->yres,
			check_timing->refresh, (check_timing->vmode &
			FB_VMODE_INTERLACED) ? true : false);

	for (i = 0; i < ARRAY_SIZE(hdmi_v13_confs); ++i)
		if (hdmi_v13_confs[i].width == check_timing->xres &&
			hdmi_v13_confs[i].height == check_timing->yres &&
			hdmi_v13_confs[i].vrefresh == check_timing->refresh &&
			hdmi_v13_confs[i].interlace ==
			((check_timing->vmode & FB_VMODE_INTERLACED) ?
			 true : false)) {
			DRM_DEBUG_KMS("[%d]x[%d] [%d]Hz [%x]:supported.\n",
				check_timing->xres, check_timing->yres,
				check_timing->refresh, check_timing->vmode);
			return 0;
		}

	DRM_DEBUG_KMS("[%d]x[%d] [%d]Hz [%x]:not supported.\n",
		check_timing->xres, check_timing->yres,
		check_timing->refresh, check_timing->vmode);
	/* TODO */

	return -EINVAL;
}

static int hdmi_v14_check_timing(struct fb_videomode *check_timing)
{
	int i;

	DRM_DEBUG_KMS("valid mode : xres=%d, yres=%d, refresh=%d, intl=%d\n",
			check_timing->xres, check_timing->yres,
			check_timing->refresh, (check_timing->vmode &
			FB_VMODE_INTERLACED) ? true : false);

	for (i = 0; i < ARRAY_SIZE(hdmi_confs); i++)
		if (hdmi_confs[i].width == check_timing->xres &&
			hdmi_confs[i].height == check_timing->yres &&
			hdmi_confs[i].vrefresh == check_timing->refresh &&
			hdmi_confs[i].interlace ==
			((check_timing->vmode & FB_VMODE_INTERLACED) ?
			 true : false)) {
			DRM_DEBUG_KMS("[%d]x[%d] [%d]Hz [%x]:supported.\n",
				check_timing->xres, check_timing->yres,
				check_timing->refresh, check_timing->vmode);
			return 0;
		}

	DRM_DEBUG_KMS("[%d]x[%d] [%d]Hz [%x]:not supported.\n",
		check_timing->xres, check_timing->yres,
		check_timing->refresh, check_timing->vmode);
	/* TODO */

	return -EINVAL;
}

static int hdmi_check_timing(void *ctx, void *timing)
{
	struct hdmi_context *hdata = ctx;
	struct fb_videomode *check_timing = timing;

	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	DRM_DEBUG_KMS("[%d]x[%d] [%d]Hz [%x]\n", check_timing->xres,
			check_timing->yres, check_timing->refresh,
			check_timing->vmode);

	if (hdata->is_v13)
		return hdmi_v13_check_timing(check_timing);
	else
		return hdmi_v14_check_timing(check_timing);
}

static void hdmi_set_acr(u32 freq, u8 *acr)
{
	u32 n, cts;

	switch (freq) {
	case 32000:
		n = 4096;
		cts = 27000;
		break;
	case 44100:
		n = 6272;
		cts = 30000;
		break;
	case 88200:
		n = 12544;
		cts = 30000;
		break;
	case 176400:
		n = 25088;
		cts = 30000;
		break;
	case 48000:
		n = 6144;
		cts = 27000;
		break;
	case 96000:
		n = 12288;
		cts = 27000;
		break;
	case 192000:
		n = 24576;
		cts = 27000;
		break;
	default:
		n = 0;
		cts = 0;
		break;
	}

	acr[1] = cts >> 16;
	acr[2] = cts >> 8 & 0xff;
	acr[3] = cts & 0xff;

	acr[4] = n >> 16;
	acr[5] = n >> 8 & 0xff;
	acr[6] = n & 0xff;
}

static void hdmi_reg_acr(struct hdmi_context *hdata, u8 *acr)
{
	hdmi_reg_writeb(hdata, HDMI_ACR_N0, acr[6]);
	hdmi_reg_writeb(hdata, HDMI_ACR_N1, acr[5]);
	hdmi_reg_writeb(hdata, HDMI_ACR_N2, acr[4]);
	hdmi_reg_writeb(hdata, HDMI_ACR_MCTS0, acr[3]);
	hdmi_reg_writeb(hdata, HDMI_ACR_MCTS1, acr[2]);
	hdmi_reg_writeb(hdata, HDMI_ACR_MCTS2, acr[1]);
	hdmi_reg_writeb(hdata, HDMI_ACR_CTS0, acr[3]);
	hdmi_reg_writeb(hdata, HDMI_ACR_CTS1, acr[2]);
	hdmi_reg_writeb(hdata, HDMI_ACR_CTS2, acr[1]);

	if (hdata->is_v13)
		hdmi_reg_writeb(hdata, HDMI_V13_ACR_CON, 4);
	else
		hdmi_reg_writeb(hdata, HDMI_ACR_CON, 4);
}

static void hdmi_audio_i2s_init(struct hdmi_context *hdata)
{
	u32 sample_rate, bits_per_sample, frame_size_code;
	u32 data_num, bit_ch, sample_frq;
	u32 val;
	u8 acr[7];

	DRM_DEBUG_KMS("%s\n", __func__);

	sample_rate = 44100;
	bits_per_sample = 16;
	frame_size_code = 0;

	switch (bits_per_sample) {
	case 20:
		data_num = 2;
		bit_ch  = 1;
		break;
	case 24:
		data_num = 3;
		bit_ch  = 1;
		break;
	default:
		data_num = 1;
		bit_ch  = 0;
		break;
	}

	hdmi_set_acr(sample_rate, acr);
	hdmi_reg_acr(hdata, acr);

	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CON, HDMI_I2S_IN_DISABLE
				| HDMI_I2S_AUD_I2S | HDMI_I2S_CUV_I2S_ENABLE
				| HDMI_I2S_MUX_ENABLE);

	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CH, HDMI_I2S_CH0_EN
			| HDMI_I2S_CH1_EN | HDMI_I2S_CH2_EN);

	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CUV, HDMI_I2S_CUV_RL_EN);

	sample_frq = (sample_rate == 44100) ? 0 :
			(sample_rate == 48000) ? 2 :
			(sample_rate == 32000) ? 3 :
			(sample_rate == 96000) ? 0xa : 0x0;

	hdmi_reg_writeb(hdata, HDMI_I2S_CLK_CON, HDMI_I2S_CLK_DIS);
	hdmi_reg_writeb(hdata, HDMI_I2S_CLK_CON, HDMI_I2S_CLK_EN);

	val = hdmi_reg_read(hdata, HDMI_I2S_DSD_CON) | 0x01;
	hdmi_reg_writeb(hdata, HDMI_I2S_DSD_CON, val);

	/* Configuration I2S input ports. Configure I2S_PIN_SEL_0~4 */
	hdmi_reg_writeb(hdata, HDMI_I2S_PIN_SEL_0, HDMI_I2S_SEL_SCLK(5)
			| HDMI_I2S_SEL_LRCK(6));
	hdmi_reg_writeb(hdata, HDMI_I2S_PIN_SEL_1, HDMI_I2S_SEL_SDATA1(1)
			| HDMI_I2S_SEL_SDATA2(4));
	hdmi_reg_writeb(hdata, HDMI_I2S_PIN_SEL_2, HDMI_I2S_SEL_SDATA3(1)
			| HDMI_I2S_SEL_SDATA2(2));
	hdmi_reg_writeb(hdata, HDMI_I2S_PIN_SEL_3, HDMI_I2S_SEL_DSD(0));

	/* I2S_CON_1 & 2 */
	hdmi_reg_writeb(hdata, HDMI_I2S_CON_1, HDMI_I2S_SCLK_FALLING_EDGE
			| HDMI_I2S_L_CH_LOW_POL);
	hdmi_reg_writeb(hdata, HDMI_I2S_CON_2, HDMI_I2S_MSB_FIRST_MODE
			| HDMI_I2S_SET_BIT_CH(bit_ch)
			| HDMI_I2S_SET_SDATA_BIT(data_num)
			| HDMI_I2S_BASIC_FORMAT);

	/* Configure register related to CUV information */
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_0, HDMI_I2S_CH_STATUS_MODE_0
			| HDMI_I2S_2AUD_CH_WITHOUT_PREEMPH
			| HDMI_I2S_COPYRIGHT
			| HDMI_I2S_LINEAR_PCM
			| HDMI_I2S_CONSUMER_FORMAT);
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_1, HDMI_I2S_CD_PLAYER);
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_2, HDMI_I2S_SET_SOURCE_NUM(0));
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_3, HDMI_I2S_CLK_ACCUR_LEVEL_2
			| HDMI_I2S_SET_SMP_FREQ(sample_frq));
	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_4,
			HDMI_I2S_ORG_SMP_FREQ_44_1
			| HDMI_I2S_WORD_LEN_MAX24_24BITS
			| HDMI_I2S_WORD_LEN_MAX_24BITS);

	hdmi_reg_writeb(hdata, HDMI_I2S_CH_ST_CON, HDMI_I2S_CH_STATUS_RELOAD);
}

static void hdmi_audio_spdif_init(struct hdmi_context *hdata)
{
	struct drm_exynos_hdmi_audio *audio = &hdata->audio;
	u32 mux_conf;
	u32 data_type = (audio->codec == HDMI_CODEC_PCM) ?
			HDMI_SPDIFIN_CFG_LINEAR_PCM_TYPE :
			(audio->codec == HDMI_CODEC_AC3) ?
			HDMI_SPDIFIN_CFG_NO_LINEAR_PCM_TYPE : 0xff;
	/* Only 4'b1011 24bit */
	u32 wl = 5 << 1 | 1;
	u32 rpt_cnt = (audio->codec == HDMI_CODEC_AC3) ? 1536 * 2 - 1 : 0;
	u32 frame_size_code = 0;

	DRM_DEBUG_KMS("%s:codec[%d]rpt_cnt[%d]\n",
		__func__, audio->codec, rpt_cnt);

	/*
	 * set configuration
	 * open SPDIF path on HDMI_I2S
	 */
	hdmi_reg_writeb(hdata , HDMI_I2S_CLK_CON, HDMI_I2S_CLK_EN);

	mux_conf = hdmi_reg_read(hdata, HDMI_I2S_MUX_CON);
	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CON, mux_conf |
		HDMI_I2S_CUV_I2S_ENABLE | HDMI_I2S_MUX_ENABLE);
	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CH, HDMI_I2S_CH_ALL_EN);
	hdmi_reg_writeb(hdata, HDMI_I2S_MUX_CUV, HDMI_I2S_CUV_RL_EN);

	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_CONFIG_1,
		HDMI_SPDIFIN_CFG_FILTER_2_SAMPLE | data_type |
		HDMI_SPDIFIN_CFG_PCPD_MANUAL_SET |
		HDMI_SPDIFIN_CFG_WORD_LENGTH_M_SET |
		HDMI_SPDIFIN_CFG_U_V_C_P_REPORT |
		HDMI_SPDIFIN_CFG_BURST_SIZE_2 |
		HDMI_SPDIFIN_CFG_DATA_ALIGN_32BIT);

	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_CONFIG_2,
		HDMI_SPDIFIN_CFG2_NO_CLK_DIV);

	/*
	 * set repetition time
	 * 24bit and manual mode
	 * if PCM this value is 0
	 */
	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_USER_VALUE_1,
		((rpt_cnt & 0xf) << 4) | wl);
	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_USER_VALUE_2,
		(rpt_cnt >> 4) & 0xff);
	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_USER_VALUE_3,
		frame_size_code & 0xff);
	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_USER_VALUE_4,
		(frame_size_code >> 8) & 0xff);

	/* enable irq */
	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_IRQ_MASK,
		HDMI_SPDIFIN_IRQ_OVERFLOW_EN);

	/* clock enable */
	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_CLK_CTRL,
		HDMI_SPDIFIN_CLK_ON);
	hdmi_reg_writeb(hdata, HDMI_SPDIFIN_OP_CTRL,
		HDMI_SPDIFIN_STATUS_CHK_OP_MODE);
}

static void hdmi_audio_init(struct hdmi_context *hdata)
{
	struct drm_exynos_hdmi_audio *audio = &hdata->audio;

	DRM_DEBUG_KMS("%s:type[%d]\n", __func__, audio->type);

	if (audio->type == HDMI_TYPE_I2S)
		hdmi_audio_i2s_init(hdata);
	else
		hdmi_audio_spdif_init(hdata);
}

static void hdmi_audio_enable(struct hdmi_context *hdata, bool enable)
{
	struct drm_exynos_hdmi_audio *audio = &hdata->audio;

	DRM_DEBUG_KMS("%s:enable[%d]dvi_mode[%d]\n", __func__,
		enable, hdata->dvi_mode);

	if (hdata->dvi_mode)
		return;

	if (enable)
		enable = audio->enable;

	DRM_DEBUG_KMS("%s:enable[%d]\n", __func__, enable);

	hdmi_reg_writeb(hdata, HDMI_AUI_CON, enable ? 2 : 0);
	hdmi_reg_writemask(hdata, HDMI_CON_0, enable ?
			HDMI_ASP_EN : HDMI_ASP_DIS, HDMI_ASP_MASK);
}

static void hdmi_audio_control(void *ctx, struct drm_exynos_hdmi_audio *audio)
{
	struct hdmi_context *hdata = ctx;

	DRM_DEBUG_KMS("%s:hpd[%d]type[%d]codec[%d]enable[%d]\n",
		__func__, atomic_read(&hdata->hpd), audio->type,
		audio->codec, audio->enable);

	hdata->audio = *audio;

	if (atomic_read(&hdata->hpd)) {
		if (audio->type == HDMI_TYPE_I2S)
			hdmi_audio_i2s_init(hdata);
		else
			hdmi_audio_spdif_init(hdata);

		hdmi_audio_enable(hdata, audio->enable);
	} else
		DRM_ERROR("failed to control audio, power down state.\n");
}

static void hdmi_conf_reset(struct hdmi_context *hdata)
{
	u32 reg;

	if (hdata->is_v13)
		reg = HDMI_V13_CORE_RSTOUT;
	else
		reg = HDMI_CORE_RSTOUT;

	/* resetting HDMI core */
	hdmi_reg_writemask(hdata, reg,  0, HDMI_CORE_SW_RSTOUT);
	mdelay(10);
	hdmi_reg_writemask(hdata, reg, ~0, HDMI_CORE_SW_RSTOUT);
	mdelay(10);
}

static void hdmi_conf_init(struct hdmi_context *hdata)
{
	/* enable HPD interrupts */
	hdmi_reg_writemask(hdata, HDMI_INTC_CON, 0, HDMI_INTC_EN_GLOBAL |
		HDMI_INTC_EN_HPD_PLUG | HDMI_INTC_EN_HPD_UNPLUG);
	mdelay(10);
	hdmi_reg_writemask(hdata, HDMI_INTC_CON, ~0, HDMI_INTC_EN_GLOBAL |
		HDMI_INTC_EN_HPD_PLUG | HDMI_INTC_EN_HPD_UNPLUG);

	/* choose HDMI mode */
	hdmi_reg_writemask(hdata, HDMI_MODE_SEL,
		HDMI_MODE_HDMI_EN, HDMI_MODE_MASK);
	/* disable bluescreen */
	hdmi_reg_writemask(hdata, HDMI_CON_0, 0, HDMI_BLUE_SCR_EN);

	if (hdata->dvi_mode) {
		/* choose DVI mode */
		hdmi_reg_writemask(hdata, HDMI_MODE_SEL,
				HDMI_MODE_DVI_EN, HDMI_MODE_MASK);
		hdmi_reg_writeb(hdata, HDMI_CON_2,
				HDMI_VID_PREAMBLE_DIS | HDMI_GUARD_BAND_DIS);
	}

	if (hdata->is_v13) {
		/* choose bluescreen (fecal) color */
		hdmi_reg_writeb(hdata, HDMI_V13_BLUE_SCREEN_0, 0x12);
		hdmi_reg_writeb(hdata, HDMI_V13_BLUE_SCREEN_1, 0x34);
		hdmi_reg_writeb(hdata, HDMI_V13_BLUE_SCREEN_2, 0x56);

		/* enable AVI packet every vsync, fixes purple line problem */
		hdmi_reg_writeb(hdata, HDMI_V13_AVI_CON, 0x02);
		/* force RGB, look to CEA-861-D, table 7 for more detail */
		hdmi_reg_writeb(hdata, HDMI_V13_AVI_BYTE(0), 0 << 5);
		hdmi_reg_writemask(hdata, HDMI_CON_1, 0x10 << 5, 0x11 << 5);

		hdmi_reg_writeb(hdata, HDMI_V13_SPD_CON, 0x02);
		hdmi_reg_writeb(hdata, HDMI_V13_AUI_CON, 0x02);
		hdmi_reg_writeb(hdata, HDMI_V13_ACR_CON, 0x04);
	} else {
		/* enable AVI packet every vsync, fixes purple line problem */
		hdmi_reg_writeb(hdata, HDMI_AVI_CON, 0x02);
		hdmi_reg_writeb(hdata, HDMI_AVI_BYTE(1), 2 << 5);
		hdmi_reg_writemask(hdata, HDMI_CON_1, 2, 3 << 5);
	}
}

static void hdmi_v13_timing_apply(struct hdmi_context *hdata)
{
	const struct hdmi_v13_preset_conf *conf =
		hdmi_v13_confs[hdata->cur_conf].conf;
	const struct hdmi_v13_core_regs *core = &conf->core;
	const struct hdmi_v13_tg_regs *tg = &conf->tg;
	int tries;

	/* setting core registers */
	hdmi_reg_writeb(hdata, HDMI_H_BLANK_0, core->h_blank[0]);
	hdmi_reg_writeb(hdata, HDMI_H_BLANK_1, core->h_blank[1]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_BLANK_0, core->v_blank[0]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_BLANK_1, core->v_blank[1]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_BLANK_2, core->v_blank[2]);
	hdmi_reg_writeb(hdata, HDMI_V13_H_V_LINE_0, core->h_v_line[0]);
	hdmi_reg_writeb(hdata, HDMI_V13_H_V_LINE_1, core->h_v_line[1]);
	hdmi_reg_writeb(hdata, HDMI_V13_H_V_LINE_2, core->h_v_line[2]);
	hdmi_reg_writeb(hdata, HDMI_VSYNC_POL, core->vsync_pol[0]);
	hdmi_reg_writeb(hdata, HDMI_INT_PRO_MODE, core->int_pro_mode[0]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_BLANK_F_0, core->v_blank_f[0]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_BLANK_F_1, core->v_blank_f[1]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_BLANK_F_2, core->v_blank_f[2]);
	hdmi_reg_writeb(hdata, HDMI_V13_H_SYNC_GEN_0, core->h_sync_gen[0]);
	hdmi_reg_writeb(hdata, HDMI_V13_H_SYNC_GEN_1, core->h_sync_gen[1]);
	hdmi_reg_writeb(hdata, HDMI_V13_H_SYNC_GEN_2, core->h_sync_gen[2]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_1_0, core->v_sync_gen1[0]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_1_1, core->v_sync_gen1[1]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_1_2, core->v_sync_gen1[2]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_2_0, core->v_sync_gen2[0]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_2_1, core->v_sync_gen2[1]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_2_2, core->v_sync_gen2[2]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_3_0, core->v_sync_gen3[0]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_3_1, core->v_sync_gen3[1]);
	hdmi_reg_writeb(hdata, HDMI_V13_V_SYNC_GEN_3_2, core->v_sync_gen3[2]);
	/* Timing generator registers */
	hdmi_reg_writeb(hdata, HDMI_TG_H_FSZ_L, tg->h_fsz_l);
	hdmi_reg_writeb(hdata, HDMI_TG_H_FSZ_H, tg->h_fsz_h);
	hdmi_reg_writeb(hdata, HDMI_TG_HACT_ST_L, tg->hact_st_l);
	hdmi_reg_writeb(hdata, HDMI_TG_HACT_ST_H, tg->hact_st_h);
	hdmi_reg_writeb(hdata, HDMI_TG_HACT_SZ_L, tg->hact_sz_l);
	hdmi_reg_writeb(hdata, HDMI_TG_HACT_SZ_H, tg->hact_sz_h);
	hdmi_reg_writeb(hdata, HDMI_TG_V_FSZ_L, tg->v_fsz_l);
	hdmi_reg_writeb(hdata, HDMI_TG_V_FSZ_H, tg->v_fsz_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_L, tg->vsync_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_H, tg->vsync_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC2_L, tg->vsync2_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC2_H, tg->vsync2_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST_L, tg->vact_st_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST_H, tg->vact_st_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_SZ_L, tg->vact_sz_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_SZ_H, tg->vact_sz_h);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_CHG_L, tg->field_chg_l);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_CHG_H, tg->field_chg_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST2_L, tg->vact_st2_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST2_H, tg->vact_st2_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_TOP_HDMI_L, tg->vsync_top_hdmi_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_TOP_HDMI_H, tg->vsync_top_hdmi_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_BOT_HDMI_L, tg->vsync_bot_hdmi_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_BOT_HDMI_H, tg->vsync_bot_hdmi_h);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_TOP_HDMI_L, tg->field_top_hdmi_l);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_TOP_HDMI_H, tg->field_top_hdmi_h);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_BOT_HDMI_L, tg->field_bot_hdmi_l);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_BOT_HDMI_H, tg->field_bot_hdmi_h);

	/* waiting for HDMIPHY's PLL to get to steady state */
	for (tries = 100; tries; --tries) {
		u32 val = hdmi_reg_read(hdata, HDMI_V13_PHY_STATUS);
		if (val & HDMI_PHY_STATUS_READY)
			break;
		mdelay(1);
	}
	/* steady state not achieved */
	if (tries == 0) {
		DRM_ERROR("hdmiphy's pll could not reach steady state.\n");
		hdmi_regs_dump(hdata, "timing apply");
	}

	clk_disable(hdata->res.sclk_hdmi);
	clk_set_parent(hdata->res.sclk_hdmi, hdata->res.sclk_hdmiphy);
	clk_enable(hdata->res.sclk_hdmi);

	/* enable HDMI and timing generator */
	hdmi_reg_writemask(hdata, HDMI_CON_0, ~0, HDMI_EN);
	if (core->int_pro_mode[0])
		hdmi_reg_writemask(hdata, HDMI_TG_CMD, ~0, HDMI_TG_EN |
				HDMI_FIELD_EN);
	else
		hdmi_reg_writemask(hdata, HDMI_TG_CMD, ~0, HDMI_TG_EN);
}

static void hdmi_v14_timing_apply(struct hdmi_context *hdata)
{
	const struct hdmi_preset_conf *conf = hdmi_confs[hdata->cur_conf].conf;
	const struct hdmi_core_regs *core = &conf->core;
	const struct hdmi_tg_regs *tg = &conf->tg;
	int tries;

	/* setting core registers */
	hdmi_reg_writeb(hdata, HDMI_H_BLANK_0, core->h_blank[0]);
	hdmi_reg_writeb(hdata, HDMI_H_BLANK_1, core->h_blank[1]);
	hdmi_reg_writeb(hdata, HDMI_V2_BLANK_0, core->v2_blank[0]);
	hdmi_reg_writeb(hdata, HDMI_V2_BLANK_1, core->v2_blank[1]);
	hdmi_reg_writeb(hdata, HDMI_V1_BLANK_0, core->v1_blank[0]);
	hdmi_reg_writeb(hdata, HDMI_V1_BLANK_1, core->v1_blank[1]);
	hdmi_reg_writeb(hdata, HDMI_V_LINE_0, core->v_line[0]);
	hdmi_reg_writeb(hdata, HDMI_V_LINE_1, core->v_line[1]);
	hdmi_reg_writeb(hdata, HDMI_H_LINE_0, core->h_line[0]);
	hdmi_reg_writeb(hdata, HDMI_H_LINE_1, core->h_line[1]);
	hdmi_reg_writeb(hdata, HDMI_HSYNC_POL, core->hsync_pol[0]);
	hdmi_reg_writeb(hdata, HDMI_VSYNC_POL, core->vsync_pol[0]);
	hdmi_reg_writeb(hdata, HDMI_INT_PRO_MODE, core->int_pro_mode[0]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F0_0, core->v_blank_f0[0]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F0_1, core->v_blank_f0[1]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F1_0, core->v_blank_f1[0]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F1_1, core->v_blank_f1[1]);
	hdmi_reg_writeb(hdata, HDMI_H_SYNC_START_0, core->h_sync_start[0]);
	hdmi_reg_writeb(hdata, HDMI_H_SYNC_START_1, core->h_sync_start[1]);
	hdmi_reg_writeb(hdata, HDMI_H_SYNC_END_0, core->h_sync_end[0]);
	hdmi_reg_writeb(hdata, HDMI_H_SYNC_END_1, core->h_sync_end[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_BEF_2_0,
			core->v_sync_line_bef_2[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_BEF_2_1,
			core->v_sync_line_bef_2[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_BEF_1_0,
			core->v_sync_line_bef_1[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_BEF_1_1,
			core->v_sync_line_bef_1[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_2_0,
			core->v_sync_line_aft_2[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_2_1,
			core->v_sync_line_aft_2[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_1_0,
			core->v_sync_line_aft_1[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_1_1,
			core->v_sync_line_aft_1[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_2_0,
			core->v_sync_line_aft_pxl_2[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_2_1,
			core->v_sync_line_aft_pxl_2[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_1_0,
			core->v_sync_line_aft_pxl_1[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_1_1,
			core->v_sync_line_aft_pxl_1[1]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F2_0, core->v_blank_f2[0]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F2_1, core->v_blank_f2[1]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F3_0, core->v_blank_f3[0]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F3_1, core->v_blank_f3[1]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F4_0, core->v_blank_f4[0]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F4_1, core->v_blank_f4[1]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F5_0, core->v_blank_f5[0]);
	hdmi_reg_writeb(hdata, HDMI_V_BLANK_F5_1, core->v_blank_f5[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_3_0,
			core->v_sync_line_aft_3[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_3_1,
			core->v_sync_line_aft_3[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_4_0,
			core->v_sync_line_aft_4[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_4_1,
			core->v_sync_line_aft_4[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_5_0,
			core->v_sync_line_aft_5[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_5_1,
			core->v_sync_line_aft_5[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_6_0,
			core->v_sync_line_aft_6[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_6_1,
			core->v_sync_line_aft_6[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_3_0,
			core->v_sync_line_aft_pxl_3[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_3_1,
			core->v_sync_line_aft_pxl_3[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_4_0,
			core->v_sync_line_aft_pxl_4[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_4_1,
			core->v_sync_line_aft_pxl_4[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_5_0,
			core->v_sync_line_aft_pxl_5[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_5_1,
			core->v_sync_line_aft_pxl_5[1]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_6_0,
			core->v_sync_line_aft_pxl_6[0]);
	hdmi_reg_writeb(hdata, HDMI_V_SYNC_LINE_AFT_PXL_6_1,
			core->v_sync_line_aft_pxl_6[1]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_1_0, core->vact_space_1[0]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_1_1, core->vact_space_1[1]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_2_0, core->vact_space_2[0]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_2_1, core->vact_space_2[1]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_3_0, core->vact_space_3[0]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_3_1, core->vact_space_3[1]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_4_0, core->vact_space_4[0]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_4_1, core->vact_space_4[1]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_5_0, core->vact_space_5[0]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_5_1, core->vact_space_5[1]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_6_0, core->vact_space_6[0]);
	hdmi_reg_writeb(hdata, HDMI_VACT_SPACE_6_1, core->vact_space_6[1]);

	/* Timing generator registers */
	hdmi_reg_writeb(hdata, HDMI_TG_H_FSZ_L, tg->h_fsz_l);
	hdmi_reg_writeb(hdata, HDMI_TG_H_FSZ_H, tg->h_fsz_h);
	hdmi_reg_writeb(hdata, HDMI_TG_HACT_ST_L, tg->hact_st_l);
	hdmi_reg_writeb(hdata, HDMI_TG_HACT_ST_H, tg->hact_st_h);
	hdmi_reg_writeb(hdata, HDMI_TG_HACT_SZ_L, tg->hact_sz_l);
	hdmi_reg_writeb(hdata, HDMI_TG_HACT_SZ_H, tg->hact_sz_h);
	hdmi_reg_writeb(hdata, HDMI_TG_V_FSZ_L, tg->v_fsz_l);
	hdmi_reg_writeb(hdata, HDMI_TG_V_FSZ_H, tg->v_fsz_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_L, tg->vsync_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_H, tg->vsync_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC2_L, tg->vsync2_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC2_H, tg->vsync2_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST_L, tg->vact_st_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST_H, tg->vact_st_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_SZ_L, tg->vact_sz_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_SZ_H, tg->vact_sz_h);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_CHG_L, tg->field_chg_l);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_CHG_H, tg->field_chg_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST2_L, tg->vact_st2_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST2_H, tg->vact_st2_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST3_L, tg->vact_st3_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST3_H, tg->vact_st3_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST4_L, tg->vact_st4_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VACT_ST4_H, tg->vact_st4_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_TOP_HDMI_L, tg->vsync_top_hdmi_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_TOP_HDMI_H, tg->vsync_top_hdmi_h);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_BOT_HDMI_L, tg->vsync_bot_hdmi_l);
	hdmi_reg_writeb(hdata, HDMI_TG_VSYNC_BOT_HDMI_H, tg->vsync_bot_hdmi_h);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_TOP_HDMI_L, tg->field_top_hdmi_l);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_TOP_HDMI_H, tg->field_top_hdmi_h);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_BOT_HDMI_L, tg->field_bot_hdmi_l);
	hdmi_reg_writeb(hdata, HDMI_TG_FIELD_BOT_HDMI_H, tg->field_bot_hdmi_h);
	hdmi_reg_writeb(hdata, HDMI_TG_3D, tg->tg_3d);

	/* waiting for HDMIPHY's PLL to get to steady state */
	for (tries = 100; tries; --tries) {
		u32 val = hdmi_reg_read(hdata, HDMI_PHY_STATUS_0);
		if (val & HDMI_PHY_STATUS_READY)
			break;
		mdelay(1);
	}
	/* steady state not achieved */
	if (tries == 0) {
		DRM_ERROR("hdmiphy's pll could not reach steady state.\n");
		hdmi_regs_dump(hdata, "timing apply");
	}

	clk_disable(hdata->res.sclk_hdmi);
	clk_set_parent(hdata->res.sclk_hdmi, hdata->res.sclk_hdmiphy);
	clk_enable(hdata->res.sclk_hdmi);

	/* enable HDMI and timing generator */
	hdmi_reg_writemask(hdata, HDMI_CON_0, ~0, HDMI_EN);
	if (core->int_pro_mode[0])
		hdmi_reg_writemask(hdata, HDMI_TG_CMD, ~0, HDMI_TG_EN |
				HDMI_FIELD_EN);
	else
		hdmi_reg_writemask(hdata, HDMI_TG_CMD, ~0, HDMI_TG_EN);
}

static void hdmi_timing_apply(struct hdmi_context *hdata)
{
	if (hdata->is_v13)
		hdmi_v13_timing_apply(hdata);
	else
		hdmi_v14_timing_apply(hdata);
}

static int hdmi_phy_ctrl(struct i2c_client *client, u8 reg, u8 bit,
		u8 *read_buffer, bool enable)
{
	int ret;
	u8 operation[2];

	operation[0] = reg;
	operation[1] = enable ? (read_buffer[reg] & (~(1 << bit))) :
			(read_buffer[reg] | (1 << bit));
	read_buffer[reg] = operation[1];

	ret = i2c_master_send(client, operation, 2);
	if (ret != 2) {
		DRM_ERROR("failed to turn %s HDMIPHY via I2C\n",
			enable ? "enable" : "disable");
		return -EIO;
	}

	return 0;
}

static int hdmi_phy_enable_oscpad(struct i2c_client *client, u8 *read_buffer,
	bool enable)
{
	u8 operation[2];
	int ret = 0;

	/* ocspad control */
	operation[0] = 0x0b;
	if (enable)
		operation[1] = 0xd8;
	else
		operation[1] = 0x18;
	read_buffer[0x0b] = operation[1];

	DRM_DEBUG_KMS("%s:write[0x%02x][0x%02x]\n",
		__func__, operation[0], operation[1]);

	ret = i2c_master_send(client, operation, 2);
	if (ret != 2) {
		DRM_ERROR("failed to %s osc pad\n",
			enable ? "enable" : "disable");
		ret = -EIO;
	}

	return ret;
}

static int hdmi_phy_power_ctrl(struct hdmi_context *hdata, bool enable)
{
	u8 operation[2];
	u8 read_buffer[32];
	int ret = 0, i;

	DRM_DEBUG_KMS("%s:enable[%d]\n", __func__, enable);

	/* read full register */
	operation[0] = 0x1;
	i2c_master_send(hdata->hdmiphy_port, operation, 1);

	memset(read_buffer, 0x0, sizeof(read_buffer));
	ret = i2c_master_recv(hdata->hdmiphy_port, read_buffer, 32);
	if (ret < 0) {
		DRM_ERROR("failed to read hdmiphy config\n");
		ret = -EIO;
		goto err_clear;
	}

	for (i = 0; i < ret; i++)
		DRM_DEBUG_KMS("hdmiphy[0x%02x] write[0x%02x] - "
			"recv [0x%02x]\n", i, operation[i], read_buffer[i]);

	if (!enable)
		hdmi_phy_enable_oscpad(hdata->hdmiphy_port, read_buffer, 0);

	hdmi_phy_ctrl(hdata->hdmiphy_port, 0x1d, 0x7, read_buffer, enable);
	hdmi_phy_ctrl(hdata->hdmiphy_port, 0x1d, 0x0, read_buffer, enable);
	hdmi_phy_ctrl(hdata->hdmiphy_port, 0x1d, 0x1, read_buffer, enable);
	hdmi_phy_ctrl(hdata->hdmiphy_port, 0x1d, 0x2, read_buffer, enable);
	hdmi_phy_ctrl(hdata->hdmiphy_port, 0x1d, 0x4, read_buffer, enable);
	hdmi_phy_ctrl(hdata->hdmiphy_port, 0x1d, 0x5, read_buffer, enable);
	hdmi_phy_ctrl(hdata->hdmiphy_port, 0x1d, 0x6, read_buffer, enable);

	if (!enable)
		hdmi_phy_ctrl(hdata->hdmiphy_port, 0x4, 0x3, read_buffer, 0);

	/* read full register */
	operation[0] = 0x1;
	i2c_master_send(hdata->hdmiphy_port, operation, 1);

	memset(read_buffer, 0x0, sizeof(read_buffer));
	ret = i2c_master_recv(hdata->hdmiphy_port, read_buffer, 32);
	if (ret < 0) {
		DRM_ERROR("failed to read hdmiphy config\n");
		ret = -EIO;
		goto err_clear;
	}

	for (i = 0; i < ret; i++)
		DRM_DEBUG_KMS("hdmiphy[0x%02x] write[0x%02x] - "
			"recv [0x%02x]\n", i, operation[i], read_buffer[i]);

	return 0;

err_clear:

	return ret;
}

static void hdmiphy_conf_reset(struct hdmi_context *hdata)
{
	u8 buffer[2];
	u32 reg;

	clk_disable(hdata->res.sclk_hdmi);
	clk_set_parent(hdata->res.sclk_hdmi, hdata->res.sclk_pixel);
	clk_enable(hdata->res.sclk_hdmi);

	/* operation mode */
	buffer[0] = 0x1f;
	buffer[1] = 0x00;

	if (hdata->hdmiphy_port)
		i2c_master_send(hdata->hdmiphy_port, buffer, 2);

	if (hdata->is_v13)
		reg = HDMI_V13_PHY_RSTOUT;
	else
		reg = HDMI_PHY_RSTOUT;

	/* reset hdmiphy */
	hdmi_reg_writemask(hdata, reg, ~0, HDMI_PHY_SW_RSTOUT);
	mdelay(10);
	hdmi_reg_writemask(hdata, reg,  0, HDMI_PHY_SW_RSTOUT);
	mdelay(10);
}

static void hdmiphy_conf_apply(struct hdmi_context *hdata)
{
	const u8 *hdmiphy_data;
	u8 buffer[32];
	u8 operation[2];
	u8 read_buffer[32] = {0, };
	int ret;
	int i;

	if (!hdata->hdmiphy_port) {
		DRM_ERROR("hdmiphy is not attached\n");
		return;
	}

	/* pixel clock */
	if (hdata->is_v13)
		hdmiphy_data = hdmi_v13_confs[hdata->cur_conf].hdmiphy_data;
	else
		hdmiphy_data = hdmi_confs[hdata->cur_conf].hdmiphy_data;

	memcpy(buffer, hdmiphy_data, 32);
	ret = i2c_master_send(hdata->hdmiphy_port, buffer, 32);
	if (ret != 32) {
		DRM_ERROR("failed to configure HDMIPHY via I2C\n");
		return;
	}

	mdelay(10);

	/* operation mode */
	operation[0] = 0x1f;
	operation[1] = 0x80;

	ret = i2c_master_send(hdata->hdmiphy_port, operation, 2);
	if (ret != 2) {
		DRM_ERROR("failed to enable hdmiphy\n");
		return;
	}

	ret = i2c_master_recv(hdata->hdmiphy_port, read_buffer, 32);
	if (ret < 0) {
		DRM_ERROR("failed to read hdmiphy config\n");
		return;
	}

	for (i = 0; i < ret; i++)
		DRM_DEBUG_KMS("hdmiphy[0x%02x] write[0x%02x] - "
			"recv [0x%02x]\n", i, buffer[i], read_buffer[i]);
}

static void hdmi_conf_apply(struct hdmi_context *hdata)
{
	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	hdmiphy_conf_reset(hdata);
	hdmiphy_conf_apply(hdata);

	mutex_lock(&hdata->hdmi_mutex);
	hdmi_conf_reset(hdata);
	hdmi_conf_init(hdata);
	mutex_unlock(&hdata->hdmi_mutex);

	hdmi_audio_init(hdata);

	/* setting core registers */
	hdmi_timing_apply(hdata);
	hdmi_audio_enable(hdata, true);

	/* HDCP Start */
	if (hdcp_ops && hdcp_ops->dpms)
		hdcp_ops->dpms(hdata->hdcp_ctx->ctx, DRM_MODE_DPMS_ON);

	hdmi_regs_dump(hdata, "start");
}

static void hdmi_mode_fixup(void *ctx, struct drm_connector *connector,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_display_mode *m;
	struct hdmi_context *hdata = ctx;
	int index;

	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	drm_mode_set_crtcinfo(adjusted_mode, 0);

	if (hdata->is_v13)
		index = hdmi_v13_conf_index(adjusted_mode);
	else
		index = hdmi_v14_conf_index(adjusted_mode);

	/* just return if user desired mode exists. */
	if (index >= 0)
		return;

	/*
	 * otherwise, find the most suitable mode among modes and change it
	 * to adjusted_mode.
	 */
	list_for_each_entry(m, &connector->modes, head) {
		if (hdata->is_v13)
			index = hdmi_v13_conf_index(m);
		else
			index = hdmi_v14_conf_index(m);

		if (index >= 0) {
			DRM_INFO("desired mode doesn't exist so\n");
			DRM_INFO("use the most suitable mode among modes.\n");
			memcpy(adjusted_mode, m, sizeof(*m));
			break;
		}
	}
}

static void hdmi_mode_set(void *ctx, void *mode)
{
	struct hdmi_context *hdata = ctx;
	int conf_idx;

	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	conf_idx = hdmi_conf_index(hdata, mode);
	if (conf_idx >= 0)
		hdata->cur_conf = conf_idx;
	else
		DRM_DEBUG_KMS("not supported mode\n");
}

static void hdmi_get_max_resol(void *ctx, unsigned int *width,
					unsigned int *height)
{
	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	*width = MAX_WIDTH;
	*height = MAX_HEIGHT;
}

static void hdmi_commit(void *ctx)
{
	struct hdmi_context *hdata = ctx;
	struct exynos_drm_private *drm_priv;
	struct exynos_drm_hdmi_context *drm_hdmi_ctx;
	struct drm_device *drm_dev;

	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	hdmi_conf_apply(hdata);

	/*
	 * parent_ctx is created at hdmi_probe() and
	 * parent_ctx->drm_dev is set at hdmi_subdrv_probe()
	 */
	drm_hdmi_ctx = hdata->parent_ctx;
	drm_dev = drm_hdmi_ctx->drm_dev;
	if (drm_dev)
		drm_priv = drm_dev->dev_private;
	else
		return;

	/*
	 * if iommu support for exynos drm was enabled, this function is
	 * called first time(!hdata->iommu_on) then enable iommu unit.
	 */
	if (drm_priv->vmm && !hdata->iommu_on) {
		int ret;

		ret = exynos_drm_iommu_activate(drm_priv->vmm, hdata->dev);
		if (ret < 0) {
			DRM_ERROR("failed to activate iommu.\n");
			return;
		}

		hdata->iommu_on = true;
	}
}

static void hdmi_poweron(struct hdmi_context *hdata)
{
	struct hdmi_resources *res = &hdata->res;
	int ret;

	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	mutex_lock(&hdata->hdmi_mutex);
	if (hdata->powered) {
		mutex_unlock(&hdata->hdmi_mutex);
		return;
	}

	hdata->regs = hdata->saved_regs;

	if (hdata->cfg_hpd)
		hdata->cfg_hpd(true);
	mutex_unlock(&hdata->hdmi_mutex);

	pm_runtime_get_sync(hdata->dev);

#ifdef CONFIG_EXTCON
	if (!hdata->extcon && hdata->extcon_name) {
		hdata->extcon = extcon_get_extcon_dev(hdata->extcon_name);
		if (!hdata->extcon)
			DRM_DEBUG_KMS("failed to get extcon device\n");
	}
#endif

	regulator_bulk_enable(res->regul_count, res->regul_bulk);
	clk_enable(res->hdmiphy);
	clk_enable(res->hdmi);
	clk_enable(res->sclk_hdmi);

	ret = hdmi_phy_power_ctrl(hdata, true);
	if (ret)
		DRM_ERROR("failed to control phy power\n");

	if (hdata->iommu_on) {
		struct exynos_drm_private *drm_priv;
		struct exynos_drm_hdmi_context *drm_hdmi_ctx;
		struct drm_device *drm_dev;

		drm_hdmi_ctx = hdata->parent_ctx;
		drm_dev = drm_hdmi_ctx->drm_dev;

		if (drm_dev)
			drm_priv = drm_dev->dev_private;
		else
			return;

		ret = exynos_drm_iommu_activate(drm_priv->vmm, hdata->dev);
		if (ret < 0) {
			DRM_ERROR("failed to activate iommu.\n");
			return;
		}
	}

	hdata->powered = true;
}

static void hdmi_cleanup(struct hdmi_context *hdata)
{
	int val, retries = 200;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* HDCP Stop */
	if (hdcp_ops && hdcp_ops->dpms)
		hdcp_ops->dpms(hdata->hdcp_ctx->ctx, DRM_MODE_DPMS_OFF);

	hdmi_reg_writemask(hdata, HDMI_CON_0, 0, HDMI_EN | HDMI_ASP_EN);

	do {
		val = hdmi_reg_read(hdata, HDMI_CON_0);
	} while ((val & HDMI_EN) && retries--);

	if (!retries)
		DRM_ERROR("hdmi disable failed.\n");
}

static void hdmi_poweroff(struct hdmi_context *hdata)
{
	struct hdmi_resources *res = &hdata->res;
	int ret;

	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	mutex_lock(&hdata->hdmi_mutex);
	if (!hdata->powered)
		goto out;

	hdata->powered = false;

	hdmi_cleanup(hdata);

	/* Mixer changed sclk_hdmi parent clock from hdmiphy to pixel */
	clk_set_parent(res->sclk_hdmi, res->sclk_hdmiphy);

	/*
	 * The TV power domain needs any condition of hdmiphy to turn off and
	 * its reset state seems to meet the condition.
	 */
	hdmiphy_conf_reset(hdata);

	ret = hdmi_phy_power_ctrl(hdata, false);
	if (ret)
		DRM_ERROR("failed to control phy power\n");

	if (hdata->iommu_on) {
		struct exynos_drm_private *drm_priv;
		struct exynos_drm_hdmi_context *drm_hdmi_ctx;
		struct drm_device *drm_dev;

		drm_hdmi_ctx = hdata->parent_ctx;
		drm_dev = drm_hdmi_ctx->drm_dev;
		if (drm_dev)
			drm_priv = drm_dev->dev_private;
		else {
			if (hdata->cfg_hpd)
				hdata->cfg_hpd(true);

			regulator_bulk_enable(res->regul_count,
						res->regul_bulk);

			DRM_ERROR("failed to get drm_dev.\n");
			clk_enable(res->hdmiphy);
			clk_enable(res->hdmi);
			clk_enable(res->sclk_hdmi);

			mutex_unlock(&hdata->hdmi_mutex);
			return;
		}

		exynos_drm_iommu_deactivate(drm_priv->vmm, hdata->dev);
	}

	clk_disable(res->sclk_hdmi);
	clk_disable(res->hdmi);
	clk_disable(res->hdmiphy);

	hdata->regs = NULL;

	regulator_bulk_disable(res->regul_count, res->regul_bulk);

	if (hdata->cfg_hpd)
		hdata->cfg_hpd(false);

	pm_runtime_put_sync(hdata->dev);

out:
	mutex_unlock(&hdata->hdmi_mutex);
}

static void hdmi_dpms(void *ctx, int mode)
{
	struct hdmi_context *hdata = ctx;

	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		hdmi_poweron(hdata);

		if (cec_ops && cec_ops->dpms)
			cec_ops->dpms(hdata->cec_ctx->ctx, mode);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		if (cec_ops && cec_ops->dpms)
			cec_ops->dpms(hdata->cec_ctx->ctx, mode);

		hdmi_poweroff(hdata);
		break;
	default:
		DRM_DEBUG_KMS("unknown dpms mode: %d\n", mode);
		break;
	}
}

static struct exynos_hdmi_ops hdmi_ops = {
	/* display */
	.is_connected	= hdmi_is_connected,
	.get_edid	= hdmi_get_edid,
	.check_timing	= hdmi_check_timing,

	/* manager */
	.mode_fixup	= hdmi_mode_fixup,
	.mode_set	= hdmi_mode_set,
	.get_max_resol	= hdmi_get_max_resol,
	.commit		= hdmi_commit,
	.dpms		= hdmi_dpms,
	.audio_control	= hdmi_audio_control,
};

static irqreturn_t hdmi_external_irq_thread(int irq, void *arg)
{
	struct exynos_drm_hdmi_context *ctx = arg;
	struct hdmi_context *hdata = ctx->ctx;
	int hpd;

	if (!hdata->get_hpd)
		goto out;

	mutex_lock(&hdata->hdmi_mutex);
	hpd = hdata->get_hpd();
	atomic_set(&hdata->hpd, hpd);
	mutex_unlock(&hdata->hdmi_mutex);

	if (ctx->drm_dev)
		drm_helper_hpd_irq_event(ctx->drm_dev);

out:
	return IRQ_HANDLED;
}

static irqreturn_t hdmi_internal_irq_thread(int irq, void *arg)
{
	struct exynos_drm_hdmi_context *ctx = arg;
	struct hdmi_context *hdata = ctx->ctx;
	u32 intc_flag;
	int hpd;

	mutex_lock(&hdata->hdmi_mutex);

	if (!hdata->powered) {
		mutex_unlock(&hdata->hdmi_mutex);
		goto out;
	}

	intc_flag = hdmi_reg_read(hdata, HDMI_INTC_FLAG);
	hpd = hdmi_reg_read(hdata, HDMI_HPD_STATUS);
	atomic_set(&hdata->hpd, hpd);

	DRM_DEBUG_KMS("%s:hpd[%d]\n", __func__, hpd);

	/* clearing flags for HPD plug/unplug */
	if (intc_flag & HDMI_INTC_FLAG_HPD_UNPLUG) {
		DRM_DEBUG_KMS("%s:unplugged\n", __func__);
		hdmi_reg_writemask(hdata, HDMI_INTC_FLAG, ~0,
			HDMI_INTC_FLAG_HPD_UNPLUG);
#ifdef CONFIG_JACK_MON
		jack_event_handler("hdmi", hpd);
#endif
	}
	if (intc_flag & HDMI_INTC_FLAG_HPD_PLUG) {
		DRM_DEBUG_KMS("%s:plugged\n", __func__);
		hdmi_reg_writemask(hdata, HDMI_INTC_FLAG, ~0,
			HDMI_INTC_FLAG_HPD_PLUG);
#ifdef CONFIG_JACK_MON
		jack_event_handler("hdmi", hpd);
#endif
	}
	if (hpd && (intc_flag & HDMI_INTC_FLAG_HDCP)) {
		DRM_DEBUG_KMS("%s:hdcp\n", __func__);
		if (hdcp_ops && hdcp_ops->commit) {
			hdcp_ops->commit(hdata->hdcp_ctx->ctx);
			hdmi_reg_writemask(hdata, HDMI_INTC_FLAG, ~0,
				HDMI_INTC_FLAG_HDCP);
		}
	}

	if (hdata->powered && hpd) {
		mutex_unlock(&hdata->hdmi_mutex);
		goto out;
	}
	mutex_unlock(&hdata->hdmi_mutex);

	if (ctx->drm_dev)
		drm_helper_hpd_irq_event(ctx->drm_dev);

out:
	return IRQ_HANDLED;
}

static int __devinit hdmi_resources_init(struct hdmi_context *hdata)
{
	struct device *dev = hdata->dev;
	struct hdmi_resources *res = &hdata->res;
	static char *supply[] = {
		/* FIXME: control HDMI_EN gpio using fixed regulator */
		/* "hdmi-en", */
		"vdd",
		"vdd_osc",
		"vdd_pll",
	};
	int i, ret;

	DRM_DEBUG_KMS("HDMI resource init\n");

	memset(res, 0, sizeof *res);

	/* get clocks, power */
	res->hdmi = clk_get(dev, "hdmi");
	if (IS_ERR_OR_NULL(res->hdmi)) {
		DRM_ERROR("failed to get clock 'hdmi'\n");
		goto fail;
	}
	res->sclk_hdmi = clk_get(dev, "sclk_hdmi");
	if (IS_ERR_OR_NULL(res->sclk_hdmi)) {
		DRM_ERROR("failed to get clock 'sclk_hdmi'\n");
		goto fail;
	}
	res->sclk_pixel = clk_get(dev, "sclk_pixel");
	if (IS_ERR_OR_NULL(res->sclk_pixel)) {
		DRM_ERROR("failed to get clock 'sclk_pixel'\n");
		goto fail;
	}
	res->sclk_hdmiphy = clk_get(dev, "sclk_hdmiphy");
	if (IS_ERR_OR_NULL(res->sclk_hdmiphy)) {
		DRM_ERROR("failed to get clock 'sclk_hdmiphy'\n");
		goto fail;
	}
	res->hdmiphy = clk_get(dev, "hdmiphy");
	if (IS_ERR_OR_NULL(res->hdmiphy)) {
		DRM_ERROR("failed to get clock 'hdmiphy'\n");
		goto fail;
	}

	clk_set_parent(res->sclk_hdmi, res->sclk_pixel);

	res->regul_bulk = kzalloc(ARRAY_SIZE(supply) *
		sizeof res->regul_bulk[0], GFP_KERNEL);
	if (!res->regul_bulk) {
		DRM_ERROR("failed to get memory for regulators\n");
		goto fail;
	}
	for (i = 0; i < ARRAY_SIZE(supply); ++i) {
		res->regul_bulk[i].supply = supply[i];
		res->regul_bulk[i].consumer = NULL;
	}
	ret = regulator_bulk_get(dev, ARRAY_SIZE(supply), res->regul_bulk);
	if (ret) {
		DRM_ERROR("failed to get regulators\n");
		goto fail;
	}
	res->regul_count = ARRAY_SIZE(supply);

	return 0;
fail:
	DRM_ERROR("HDMI resource init - failed\n");
	return -ENODEV;
}

static int hdmi_resources_cleanup(struct hdmi_context *hdata)
{
	struct hdmi_resources *res = &hdata->res;

	regulator_bulk_free(res->regul_count, res->regul_bulk);
	/* kfree is NULL-safe */
	kfree(res->regul_bulk);
	if (!IS_ERR_OR_NULL(res->hdmiphy))
		clk_put(res->hdmiphy);
	if (!IS_ERR_OR_NULL(res->sclk_hdmiphy))
		clk_put(res->sclk_hdmiphy);
	if (!IS_ERR_OR_NULL(res->sclk_pixel))
		clk_put(res->sclk_pixel);
	if (!IS_ERR_OR_NULL(res->sclk_hdmi))
		clk_put(res->sclk_hdmi);
	if (!IS_ERR_OR_NULL(res->hdmi))
		clk_put(res->hdmi);
	memset(res, 0, sizeof *res);

	return 0;
}

static struct i2c_client *hdmi_ddc, *hdmi_hdmiphy;
static struct device *cec_dev;

void exynos_hdmi_attach_ddc_client(struct i2c_client *ddc)
{
	if (ddc)
		hdmi_ddc = ddc;
}

void exynos_hdmi_attach_hdmiphy_client(struct i2c_client *hdmiphy)
{
	if (hdmiphy)
		hdmi_hdmiphy = hdmiphy;
}

void exynos_hdmi_attach_cec_dev(struct device *dev)
{
	if (dev)
		cec_dev = dev;
}

static int __devinit hdmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_drm_hdmi_context *drm_hdmi_ctx;
	struct hdmi_context *hdata;
	struct exynos_drm_hdmi_pdata *pdata;
	struct drm_exynos_hdmi_audio *audio;
	struct resource *res;
	int ret;

	DRM_DEBUG_KMS("[%d]\n", __LINE__);

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		DRM_ERROR("no platform data specified\n");
		return -EINVAL;
	}

	drm_hdmi_ctx = kzalloc(sizeof(*drm_hdmi_ctx), GFP_KERNEL);
	if (!drm_hdmi_ctx) {
		DRM_ERROR("failed to allocate common hdmi context.\n");
		return -ENOMEM;
	}

	hdata = kzalloc(sizeof(struct hdmi_context), GFP_KERNEL);
	if (!hdata) {
		DRM_ERROR("out of memory\n");
		kfree(drm_hdmi_ctx);
		return -ENOMEM;
	}

	mutex_init(&hdata->hdmi_mutex);

	drm_hdmi_ctx->ctx = (void *)hdata;
	hdata->parent_ctx = (void *)drm_hdmi_ctx;

	platform_set_drvdata(pdev, drm_hdmi_ctx);

	hdata->is_v13 = pdata->is_v13;
	hdata->cfg_hpd = pdata->cfg_hpd;
	hdata->get_hpd = pdata->get_hpd;
#ifdef CONFIG_EXTCON
	hdata->extcon_name = pdata->extcon_name;
#endif
	hdata->dev = dev;

	/* audio configuration */
	audio = &hdata->audio;
	audio->type = HDMI_TYPE_I2S;
	audio->codec = HDMI_CODEC_PCM;
	audio->enable = true;

	ret = hdmi_resources_init(hdata);
	if (ret) {
		ret = -EINVAL;
		goto err_data;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		DRM_ERROR("failed to find registers\n");
		ret = -ENOENT;
		goto err_resource;
	}

	hdata->regs_res = request_mem_region(res->start, resource_size(res),
					   dev_name(dev));
	if (!hdata->regs_res) {
		DRM_ERROR("failed to claim register region\n");
		ret = -ENOENT;
		goto err_resource;
	}

	hdata->regs = ioremap(res->start, resource_size(res));
	if (!hdata->regs) {
		DRM_ERROR("failed to map registers\n");
		ret = -ENXIO;
		goto err_req_region;
	}
	hdata->saved_regs = hdata->regs;

	/* DDC i2c driver */
	if (i2c_add_driver(&hdmi_ddc_driver)) {
		DRM_ERROR("failed to register ddc i2c driver\n");
		ret = -ENOENT;
		goto err_iomap;
	}

	hdata->ddc_port = hdmi_ddc;

	/* hdmiphy i2c driver */
	if (i2c_add_driver(&hdmi_phy_driver)) {
		DRM_ERROR("failed to register hdmiphy i2c driver\n");
		ret = -ENOENT;
		goto err_ddc;
	}

	hdata->hdmiphy_port = hdmi_hdmiphy;

	/*
	 * HDMI PHY power off
	 * HDMI PHY is on as default configuration
	 * So, HDMI PHY must be turned off if it's not used
	 */
	clk_enable(hdata->res.hdmiphy);
	ret = hdmi_phy_power_ctrl(hdata, false);
	clk_disable(hdata->res.hdmiphy);
	if (ret) {
		DRM_ERROR("failed to control phy power\n");
		goto err_hdmiphy;
	}

	hdata->external_irq = platform_get_irq_byname(pdev, "external_irq");
	if (hdata->external_irq < 0) {
		DRM_ERROR("failed to get platform irq\n");
		ret = hdata->external_irq;
		goto err_hdmiphy;
	}

	hdata->internal_irq = platform_get_irq_byname(pdev, "internal_irq");
	if (hdata->internal_irq < 0) {
		DRM_ERROR("failed to get platform internal irq\n");
		ret = hdata->internal_irq;
		goto err_hdmiphy;
	}

	ret = request_threaded_irq(hdata->external_irq, NULL,
			hdmi_external_irq_thread, IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"hdmi_external", drm_hdmi_ctx);
	if (ret) {
		DRM_ERROR("failed to register hdmi internal interrupt\n");
		goto err_hdmiphy;
	}

	if (hdata->cfg_hpd)
		hdata->cfg_hpd(false);

	ret = request_threaded_irq(hdata->internal_irq, NULL,
			hdmi_internal_irq_thread, IRQF_ONESHOT,
			"hdmi_internal", drm_hdmi_ctx);
	if (ret) {
		DRM_ERROR("failed to register hdmi internal interrupt\n");
		goto err_free_irq;
	}

	if (pdata->hdcp_dev) {
		hdata->hdcp_ctx = (struct exynos_drm_hdmi_context *)
					get_hdmi_context(pdata->hdcp_dev);
		if (!hdata->hdcp_ctx) {
			DRM_ERROR("hdcp context is null.\n");
			ret = -EFAULT;
			goto err_free_irq;
		}
	}

	if (pdata->cec_dev) {
		hdata->cec_ctx = (struct exynos_drm_hdmi_context *)
					get_hdmi_context(pdata->cec_dev);
		if (!hdata->cec_ctx) {
			DRM_DEBUG_KMS("cec context is null.\n");
			ret = -EFAULT;
			goto err_free_irq;
		}
	}

	/* register specific callbacks to common hdmi. */
	exynos_hdmi_ops_register(&hdmi_ops);

	/* register regs for hdcp */
	if (hdata->hdcp_ctx) {
		if (exynos_hdcp_register(hdata->hdcp_ctx->ctx, hdata->regs))
			hdcp_ops = NULL;
	}

	pm_runtime_enable(dev);

	dev_info(dev, "drm hdmi registered successfully.\n");

	return 0;

err_free_irq:
	free_irq(hdata->external_irq, drm_hdmi_ctx);
err_hdmiphy:
	i2c_del_driver(&hdmi_phy_driver);
err_ddc:
	i2c_del_driver(&hdmi_ddc_driver);
err_iomap:
	iounmap(hdata->regs);
err_req_region:
	release_mem_region(hdata->regs_res->start,
			resource_size(hdata->regs_res));
err_resource:
	hdmi_resources_cleanup(hdata);
err_data:
	kfree(hdata);
	kfree(drm_hdmi_ctx);
	return ret;
}

static int __devexit hdmi_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_drm_hdmi_context *ctx = platform_get_drvdata(pdev);
	struct hdmi_context *hdata = ctx->ctx;

	DRM_DEBUG_KMS("[%d] %s\n", __LINE__, __func__);

	pm_runtime_disable(dev);

	free_irq(hdata->internal_irq, hdata);

	hdmi_resources_cleanup(hdata);

	iounmap(hdata->regs);

	release_mem_region(hdata->regs_res->start,
			resource_size(hdata->regs_res));

	/* hdmiphy i2c driver */
	i2c_del_driver(&hdmi_phy_driver);
	/* DDC i2c driver */
	i2c_del_driver(&hdmi_ddc_driver);

	kfree(hdata);
	kfree(ctx);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int hdmi_suspend(struct device *dev)
{
	struct exynos_drm_hdmi_context *ctx = get_hdmi_context(dev);
	struct hdmi_context *hdata = ctx->ctx;

	disable_irq(hdata->internal_irq);
	disable_irq(hdata->external_irq);

	atomic_set(&hdata->hpd, 0);
	if (ctx->drm_dev)
		drm_helper_hpd_irq_event(ctx->drm_dev);

	hdmi_poweroff(hdata);

	return 0;
}

static int hdmi_resume(struct device *dev)
{
	struct exynos_drm_hdmi_context *ctx = get_hdmi_context(dev);
	struct hdmi_context *hdata = ctx->ctx;

	enable_irq(hdata->external_irq);
	enable_irq(hdata->internal_irq);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(hdmi_pm_ops, hdmi_suspend, hdmi_resume);

struct platform_driver hdmi_driver = {
	.probe		= hdmi_probe,
	.remove		= __devexit_p(hdmi_remove),
	.driver		= {
		.name	= "exynos4-hdmi",
		.owner	= THIS_MODULE,
		.pm	= &hdmi_pm_ops,
	},
};
