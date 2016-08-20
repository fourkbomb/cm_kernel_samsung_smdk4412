/*
 * linux/arch/arm/mach-exynos/board-slp-pq.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/mmc/host.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/wm8994/pdata.h>
#ifdef CONFIG_LEDS_AAT1290A
#include <linux/leds-aat1290a.h>
#endif
#include <linux/lcd.h>
#include <linux/lcd-property.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/sensor/sensors_core.h>
#include <linux/sensor/lsm330dlc_accel.h>
#include <linux/sensor/lsm330dlc_gyro.h>
#include <linux/sensor/ak8963.h>
#include <linux/sensor/ak8975.h>
#include <linux/sensor/gp2ap020.h>
#include <linux/sensor/cm36651.h>
#include <linux/cma.h>
#include <linux/jack.h>
#include <linux/uart_select.h>
#include <linux/utsname.h>
#include <linux/mfd/max77686.h>
#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>
#include <linux/leds-max77693.h>
#include <linux/battery/max17047_fuelgauge.h>
#include <linux/power/charger-manager.h>
#include <linux/sensor/lps331ap.h>
#include <linux/devfreq/exynos4_bus.h>
#include <linux/pm_qos_params.h>
#include <drm/exynos_drm.h>
#include <linux/printk.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/exynos4.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/pd.h>
#include <plat/sdhci.h>
#include <plat/mshci.h>
#include <plat/ehci.h>
#include <plat/usbgadget.h>
#include <plat/s3c64xx-spi.h>
#include <plat/csis.h>
#include <plat/udc-hs.h>
#include <plat/media.h>
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
#include <media/exynos_fimc_is.h>
#endif
#include <plat/regs-fb.h>
#include <plat/fb-core.h>
#include <plat/mipi_dsim2.h>
#include <plat/fimd_lite_ext.h>
#include <plat/hdmi.h>
#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC) || defined(CONFIG_VIDEO_MFC5X)
#include <plat/s5p-mfc.h>
#endif

#ifdef CONFIG_I2C_SI4705
#include <linux/si4705_pdata.h>
#endif

#ifdef CONFIG_VIDEO_JPEG_V2X
#include <plat/jpeg.h>
#endif

#include <mach/map.h>
#include <mach/spi-clocks.h>
#include <mach/sec_debug.h>

#ifdef CONFIG_SND_SOC_WM8994
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>
#endif

#include <mach/trats2-power.h>
#include <mach/board-melfas.h>

#include <mach/dwmci.h>

#include <mach/bcm47511.h>

#include <mach/regs-pmu.h>
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#include <mach/secmem.h>
#endif

#include <../../../drivers/video/samsung/s3cfb.h>
#include <mach/dev-sysmmu.h>

#include "board-mobile.h"

#ifdef CONFIG_EXYNOS4_SETUP_THERMAL
#include <plat/s5p-tmu.h>
#include <mach/regs-tmu.h>
#endif

#ifdef CONFIG_BUSFREQ_OPP
#include <mach/dev.h>
#include <mach/ppmu.h>
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/power_supply.h>
#include <linux/battery/samsung_battery.h>
#endif
#include <mach/trats2-thermistor.h>

#include <linux/host_notify.h>

#ifdef CONFIG_INPUT_SECBRIDGE
#include <linux/input/sec-input-bridge.h>
#endif
#ifdef CONFIG_DRM_EXYNOS_FIMC
#include <linux/dma-mapping.h>
#endif

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
#include <plat/fimc-core.h>
#include <media/s5p_fimc.h>
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
#include <media/exynos_flite.h>
#endif

#if defined(CONFIG_VIDEO_SLP_S5C73M3)
#include <media/s5c73m3_platform.h>
#endif

enum gpio_i2c {
	I2C_LAST_HW	= 8, /* I2C0~8 are reserved */
	I2C_CODEC	= 9, /* I2C9 is reserved for CODEC (hardcoded) */
	I2C_NFC,
	I2C_3_TOUCH,
	I2C_FUEL,
	I2C_BSENSE,
	I2C_MSENSE,
	I2C_MHL		= 15, /* 15 is hardcoded from midas-mhl.c */
	I2C_MHL_D	= 16, /* 16 is hardcoded from midas-mhl.c */
	I2C_PSENSE,
	I2C_IF_PMIC,
	I2C_FM_RADIO	= 19, /* refer from midas */
};

enum {
	BOARD_M0,
	BOARD_REDWOOD,
};

enum board_rev {
	M0_PROXIMA_REV0_0 = 0x3,
	M0_PROXIMA_REV0_1 = 0x0,
	M0_REAL_REV0_6 = 0x7,
	M0_REAL_REV0_6_A = 0x8,
	SLP_PQ_CMC221_LTE = 0x2,
	M0_REAL_REV1_0 = 0xb,
	M0_REAL_REV1_1 = 0xc,
};

enum redwood_board_rev {
	REDWOOD_REV0_1_0425 = 0x1c,
	REDWOOD_REV0_1_0704 = 0x14,
	REDWOOD_REAL_REV0_0 = 0x16,
	REDWOOD_REAL_REV0_1 = 0x17,
	REDWOOD_REAL_REV0_2 = 0x18,
};

static int hwrevision(int rev)
{
	switch (rev) {
	case M0_PROXIMA_REV0_0:
	case M0_PROXIMA_REV0_1:
	case M0_REAL_REV0_6:
	case M0_REAL_REV0_6_A:
	case SLP_PQ_CMC221_LTE:
	case M0_REAL_REV1_0:
	case M0_REAL_REV1_1:
	case REDWOOD_REV0_1_0425:
	case REDWOOD_REV0_1_0704:
	case REDWOOD_REAL_REV0_0:
	case REDWOOD_REAL_REV0_1:
	case REDWOOD_REAL_REV0_2:
		return (rev == system_rev);
	}
	return 0;
}

static inline int board_is_m0(void)
{
	return ((system_rev >> 4) & 0xf) == BOARD_M0;
}

static inline int board_is_redwood(void)
{
	return ((system_rev >> 4) & 0xf) == BOARD_REDWOOD;
}

extern int brcm_wlan_init(void);
/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SLP_MIDAS_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SLP_MIDAS_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SLP_MIDAS_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg slp_midas_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SLP_MIDAS_UCON_DEFAULT,
		.ulcon		= SLP_MIDAS_ULCON_DEFAULT,
		.ufcon		= SLP_MIDAS_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SLP_MIDAS_UCON_DEFAULT,
		.ulcon		= SLP_MIDAS_ULCON_DEFAULT,
		.ufcon		= SLP_MIDAS_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SLP_MIDAS_UCON_DEFAULT,
		.ulcon		= SLP_MIDAS_ULCON_DEFAULT,
		.ufcon		= SLP_MIDAS_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SLP_MIDAS_UCON_DEFAULT,
		.ulcon		= SLP_MIDAS_ULCON_DEFAULT,
		.ufcon		= SLP_MIDAS_UFCON_DEFAULT,
	},
};

#if defined(CONFIG_S3C64XX_DEV_SPI)
static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line = EXYNOS4_GPB(5),
		.set_level = gpio_set_value,
		.fb_delay = 0x2,
	},
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias = "s5c73m3_spi",
		.platform_data = NULL,
		.max_speed_hz = 50000000,
		.bus_num = 1,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.controller_data = &spi1_csi[0],
	}
};
#endif

/* camera_class should be initialized once. */
struct class *camera_class;

static int __init camera_class_init(void)
{
	camera_class = class_create(THIS_MODULE, "camera");
	if (IS_ERR(camera_class)) {
		pr_err("Failed to create class(camera)!\n");
		return PTR_ERR(camera_class);
	}

	return 0;
}

subsys_initcall(camera_class_init);

#define FRONT_CAM_MCLK_DEVIDED_REVISION	0x08
#define USE_8M_CAM_SENSOR_CORE_REVISION	0xFF

#if defined(CONFIG_VIDEO_FIMC)
/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * This function also called at fimc_init_camera()
 * Do optimization for cameras on your platform.
 */

#define CAM_CHECK_ERR_RET(x, msg)					\
	if (unlikely((x) < 0)) {					\
		printk(KERN_ERR "\nfail to %s: err = %d\n", msg, x);	\
		return x;						\
	}
#define CAM_CHECK_ERR(x, msg)						\
	if (unlikely((x) < 0)) {					\
		printk(KERN_ERR "\nfail to %s: err = %d\n", msg, x);	\
	}
#define CAM_CHECK_ERR_GOTO(x, out, fmt, ...) \
	if (unlikely((x) < 0)) { \
		printk(KERN_ERR fmt, ##__VA_ARGS__); \
		goto out; \
	}

static int s3c_csis_power(int enable)
{
	struct regulator *regulator;
	int ret = 0;

	/* mipi_1.1v ,mipi_1.8v are always powered-on.
	 * If they are off, we then power them on.
	 */
	if (enable) {
		/* VMIPI_1.0V */
		regulator = regulator_get(NULL, "vmipi_1.0v");
		if (IS_ERR(regulator))
			goto error_out;
		regulator_enable(regulator);
		regulator_put(regulator);

		/* VMIPI_1.8V */
		regulator = regulator_get(NULL, "vmipi_1.8v");
		if (IS_ERR(regulator))
			goto error_out;
		regulator_enable(regulator);
		regulator_put(regulator);

		printk(KERN_WARNING "%s: vmipi_1.0v and vmipi_1.8v were ON\n",
		       __func__);
	} else {
		/* VMIPI_1.8V */
		regulator = regulator_get(NULL, "vmipi_1.8v");
		if (IS_ERR(regulator))
			goto error_out;
		if (regulator_is_enabled(regulator)) {
			printk(KERN_WARNING "%s: vmipi_1.8v is on. so OFF\n",
			       __func__);
			ret = regulator_disable(regulator);
		}
		regulator_put(regulator);

		/* VMIPI_1.0V */
		regulator = regulator_get(NULL, "vmipi_1.0v");
		if (IS_ERR(regulator))
			goto error_out;
		if (regulator_is_enabled(regulator)) {
			printk(KERN_WARNING "%s: vmipi_1.1v is on. so OFF\n",
			       __func__);
			ret = regulator_disable(regulator);
		}
		regulator_put(regulator);

		printk(KERN_WARNING "%s: vmipi_1.0v and vmipi_1.8v were OFF\n",
		       __func__);
	}

	return 0;

error_out:
	printk(KERN_ERR "%s: ERROR: failed to check mipi-power\n", __func__);
	return 0;
}

#ifdef CONFIG_WRITEBACK_ENABLED
#define WRITEBACK_ENABLED
#endif
#ifdef WRITEBACK_ENABLED
static int get_i2c_busnum_writeback(void)
{
	return 0;
}

static struct i2c_board_info writeback_i2c_info = {
	I2C_BOARD_INFO("WriteBack", 0x0),
};

static struct s3c_platform_camera writeback = {
	.id		= CAMERA_WB,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.get_i2c_busnum = get_i2c_busnum_writeback,
	.info		= &writeback_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUV444,
	.line_length	= 1280,
	.width		= 720,
	.height		= 1280,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 720,
		.height	= 1280,
	},

	.initialized	= 0,
};
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
#ifdef CONFIG_VIDEO_S5K6A3
static int s5k6a3_gpio_request(void)
{
	int ret = 0;

	/* SENSOR_A2.8V */
	ret = gpio_request(GPIO_CAM_IO_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_CAM_IO_EN)\n");
		return ret;
	}

#if defined(CONFIG_MACH_T0) || defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_SLP_T0_LTE)
	ret = gpio_request(GPIO_VTCAM_MCLK, "GPM2");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_VTCAM_MCLK)\n");
		return ret;
	}
#else
	if (system_rev <= FRONT_CAM_MCLK_DEVIDED_REVISION)
		ret = gpio_request(GPIO_CAM_MCLK, "GPJ1");
	else
		ret = gpio_request(GPIO_VTCAM_MCLK, "GPM2");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_VTCAM_MCLK)\n");
		return ret;
	}
#endif

	ret = gpio_request(GPIO_CAM_VT_nRST, "GPM1");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_CAM_VT_nRST)\n");
		return ret;
	}

	return ret;
}

static int s5k6a3_power_on(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	s5k6a3_gpio_request();

	/* CAM_SENSOR_A2.8V */
	ret = gpio_direction_output(GPIO_CAM_IO_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_IO_EN");
	/* delay is needed : external LDO control is slower than MCLK control*/
	udelay(100);

	/* MCLK */
#if defined(CONFIG_MACH_T0) || defined(CONFIG_MACH_M3) ||\
	defined(CONFIG_MACH_SLP_T0_LTE)
	ret = s3c_gpio_cfgpin(GPIO_VTCAM_MCLK, S3C_GPIO_SFN(3));
	s3c_gpio_setpull(GPIO_VTCAM_MCLK, S3C_GPIO_PULL_NONE);
#if defined(CONFIG_MACH_T0)  || defined(CONFIG_MACH_SLP_T0_LTE)
	s5p_gpio_set_drvstr(GPIO_VTCAM_MCLK, S5P_GPIO_DRVSTR_LV1);
#else
	s5p_gpio_set_drvstr(GPIO_VTCAM_MCLK, S5P_GPIO_DRVSTR_LV2);
#endif
#else
	if (system_rev <= FRONT_CAM_MCLK_DEVIDED_REVISION) {
		ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(GPIO_CAM_MCLK, S5P_GPIO_DRVSTR_LV2);
	} else {
		ret = s3c_gpio_cfgpin(GPIO_VTCAM_MCLK, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(GPIO_VTCAM_MCLK, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(GPIO_VTCAM_MCLK, S5P_GPIO_DRVSTR_LV2);
	}
#endif
	CAM_CHECK_ERR_RET(ret, "cfg mclk");

	/* VT_RESET */
	ret = gpio_direction_output(GPIO_CAM_VT_nRST, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_VT_nRST");

	/* VT_CORE_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable vt_cam_1.8v");

	gpio_free(GPIO_CAM_IO_EN);
	gpio_free(GPIO_CAM_VT_nRST);

#if defined(CONFIG_MACH_T0) || defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_SLP_T0_LTE)
	gpio_free(GPIO_VTCAM_MCLK);
#else
	if (system_rev <= FRONT_CAM_MCLK_DEVIDED_REVISION)
		gpio_free(GPIO_CAM_MCLK);
	else
		gpio_free(GPIO_VTCAM_MCLK);
#endif

	return ret;
}

static int s5k6a3_power_down(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	s5k6a3_gpio_request();

	/* VT_RESET */
	ret = gpio_direction_output(GPIO_CAM_VT_nRST, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_VT_nRST");

	/* VT_CORE_1.8V */
	regulator = regulator_get(NULL, "vt_cam_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable vt_cam_1.8v");

	/* CAM_SENSOR_A2.8V */
	ret = gpio_direction_output(GPIO_CAM_IO_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_IO_EN");
	/* delay is needed : external LDO control is slower than MCLK control*/
	udelay(500);

	/* MCLK */
#if defined(CONFIG_MACH_T0) || defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_SLP_T0_LTE)
	ret = s3c_gpio_cfgpin(GPIO_VTCAM_MCLK, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_VTCAM_MCLK, S3C_GPIO_PULL_DOWN);
#else
	if (system_rev <= FRONT_CAM_MCLK_DEVIDED_REVISION) {
		ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_DOWN);

	} else {
		ret = s3c_gpio_cfgpin(GPIO_VTCAM_MCLK, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_VTCAM_MCLK, S3C_GPIO_PULL_DOWN);
	}
#endif
	CAM_CHECK_ERR(ret, "cfg mclk");

	gpio_free(GPIO_CAM_IO_EN);
	gpio_free(GPIO_CAM_VT_nRST);

#if defined(CONFIG_MACH_T0) || defined(CONFIG_MACH_M3) || \
	defined(CONFIG_MACH_SLP_T0_LTE)
	gpio_free(GPIO_VTCAM_MCLK);
#else
	if (system_rev <= FRONT_CAM_MCLK_DEVIDED_REVISION)
		gpio_free(GPIO_CAM_MCLK);
	else
		gpio_free(GPIO_VTCAM_MCLK);
#endif

	return ret;
}


static int s5k6a3_power(int enable)
{
	int ret = 0;

	if (enable) {
		ret = s5k6a3_power_on();
		if (unlikely(ret)) {
			printk(KERN_ERR "%s: power-on fail\n", __func__);
			goto error_out;
		}
	} else
		ret = s5k6a3_power_down();

	ret = s3c_csis_power(enable);

error_out:
	return ret;
}

static const char *s5k6a3_get_clk_name(void)
{
#if defined(CONFIG_MACH_P4NOTE) || \
	defined(CONFIG_MACH_T0) || defined(CONFIG_MACH_M3) \
	|| defined(CONFIG_MACH_SLP_T0_LTE)
	return "sclk_cam1";
#else
	if (system_rev <= FRONT_CAM_MCLK_DEVIDED_REVISION)
		return "sclk_cam0";
	else
		return "sclk_cam1";
#endif
}

static struct s3c_platform_camera s5k6a3 = {
	.id		= CAMERA_CSI_D,
	.get_clk_name = s5k6a3_get_clk_name,
	.cam_power	= s5k6a3_power,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_RAW10,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.mipi_lanes	= 1,
	.mipi_settle	= 18,
	.mipi_align	= 24,

	.initialized	= 0,
	.flite_id	= FLITE_IDX_B,
	.use_isp	= true,
	.sensor_index	= 102,
};

#ifdef CONFIG_S5K6A3_CSI_D
static struct s3c_platform_camera s5k6a3_fd = {
	.id		= CAMERA_CSI_D,
	.get_clk_name = s5k6a3_get_clk_name,
	.cam_power	= s5k6a3_power,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_RAW10,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.mipi_lanes	= 1,
	.mipi_settle	= 18,
	.mipi_align	= 24,

	.initialized	= 0,
	.flite_id	= FLITE_IDX_B,
	.use_isp	= true,
	.sensor_index	= 200,
};
#endif
#endif
static struct exynos4_platform_fimc_is exynos4_fimc_is_plat  = {
	.hw_ver = 15,
	.phy_rotate = 3,
};

static struct exynos4_platform_fimc_is exynos4_fimc_is_plat_old  = {
	.hw_ver = 15,
	.phy_rotate = 1,
};
#endif

#if defined(CONFIG_VIDEO_SLP_S5C73M3)
static int vddCore = 1150000;
static bool isVddCoreSet;
static void s5c73m3_set_vdd_core(int level)
{
	vddCore = level;
	isVddCoreSet = true;
	printk(KERN_ERR "%s : %d\n", __func__, vddCore);
}

static bool s5c73m3_is_vdd_core_set(void)
{
	return isVddCoreSet;
}

static int s5c73m3_is_isp_reset(void)
{
	int ret = 0;

		ret = gpio_request(GPIO_ISP_RESET, "GPF1");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
		return ret;
	}

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_RESET");
	udelay(10);	/* 200 cycle */
	ret = gpio_direction_output(GPIO_ISP_RESET, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_RESET");
	udelay(10);	/* 200 cycle */

	gpio_free(GPIO_ISP_RESET);

	return ret;
}

static int s5c73m3_gpio_request(void)
{
	int ret = 0;

	ret = gpio_request(GPIO_ISP_STANDBY, "GPM0");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_STANDBY)\n");
		return ret;
	}

	ret = gpio_request(GPIO_ISP_RESET, "GPF1");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
		return ret;
	}

	/* SENSOR_A2.8V */
	ret = gpio_request(GPIO_CAM_IO_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_CAM_IO_EN)\n");
		return ret;
	}

	ret = gpio_request(GPIO_CAM_AF_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_AF_EN)\n");
		return ret;
	}

	ret = gpio_request(GPIO_ISP_CORE_EN, "GPM0");
	if (ret) {
		printk(KERN_ERR "fail to request gpio(GPIO_ISP_CORE_EN)\n");
		return ret;
	}

#if defined(CONFIG_MACH_C1) || defined(CONFIG_MACH_T0) || \
	defined(CONFIG_MACH_SLP_T0_LTE)
	if (system_rev >= USE_8M_CAM_SENSOR_CORE_REVISION) {
		ret = gpio_request(GPIO_CAM_SENSOR_CORE_EN, "GPM0");
		if (ret) {
			printk(KERN_ERR "fail to request gpio(GPIO_CAM_SENSOR_CORE_EN)\n");
			return ret;
		}
	}
#endif

	return ret;
}

static void s5c73m3_gpio_free(void)
{
	gpio_free(GPIO_ISP_STANDBY);
	gpio_free(GPIO_ISP_RESET);
	gpio_free(GPIO_CAM_IO_EN);
	gpio_free(GPIO_CAM_AF_EN);
	gpio_free(GPIO_ISP_CORE_EN);

#if defined(CONFIG_MACH_C1) || defined(CONFIG_MACH_T0) || \
	defined(CONFIG_MACH_SLP_T0_LTE)
	if (system_rev >= USE_8M_CAM_SENSOR_CORE_REVISION)
		gpio_free(GPIO_CAM_SENSOR_CORE_EN);
#endif
}

static int s5c73m3_power_on(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	s5c73m3_gpio_request();

	/* CAM_ISP_CORE_1.2V */
	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_CORE_EN");

	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	regulator_set_voltage(regulator, vddCore, vddCore);
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_core_1.2v");

	/* CAM_SENSOR_A2.8V */
	ret = gpio_direction_output(GPIO_CAM_IO_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output IO_EN");

	/* CAM_SENSOR_CORE_1.2V */
#if defined(CONFIG_MACH_C1) || defined(CONFIG_MACH_T0) || \
	defined(CONFIG_MACH_SLP_T0_LTE)
	printk(KERN_DEBUG "system_rev : %d\n", system_rev);
	if (system_rev >= USE_8M_CAM_SENSOR_CORE_REVISION) {
		ret = gpio_direction_output(GPIO_CAM_SENSOR_CORE_EN, 1);
		CAM_CHECK_ERR_RET(ret, "output CAM_SENSOR_CORE_EN");
		/* delay is needed : external LDO is slower than MCLK control*/
		udelay(200);
	} else {
		regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
		if (IS_ERR(regulator))
			return -ENODEV;
		ret = regulator_enable(regulator);
		regulator_put(regulator);
		CAM_CHECK_ERR_RET(ret, "enable cam_sensor_core_1.2v");
		/* delay is needed : pmu control is slower than gpio control*/
		mdelay(5);
	}
#else
	regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_sensor_core_1.2v");
       /* delay is needed : pmu control is slower than gpio control*/
	mdelay(5);
#endif

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));
	CAM_CHECK_ERR_RET(ret, "cfg mclk");
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_NONE);
#if defined(CONFIG_MACH_T0) || defined(CONFIG_MACH_SLP_T0_LTE)
	s5p_gpio_set_drvstr(GPIO_CAM_MCLK, S5P_GPIO_DRVSTR_LV2);
#else
	s5p_gpio_set_drvstr(GPIO_CAM_MCLK, S5P_GPIO_DRVSTR_LV3);
#endif

	/* CAM_AF_2.8V */
	ret = gpio_direction_output(GPIO_CAM_AF_EN, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_AF_EN");
	udelay(2000);

	/* CAM_ISP_SENSOR_1.8V */
	regulator = regulator_get(NULL, "cam_isp_sensor_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_sensor_1.8v");

	/* CAM_ISP_MIPI_1.2V */
	regulator = regulator_get(NULL, "cam_isp_mipi_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "enable cam_isp_mipi_1.2v");
       /* delay is needed : pmu control is slower than gpio control*/
	mdelay(5);

	/* ISP_STANDBY */
	ret = gpio_direction_output(GPIO_ISP_STANDBY, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_STANDBY");
	udelay(100);		/* 2000 cycle */

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 1);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_RESET");
	udelay(10);		/* 200 cycle */

	s5c73m3_gpio_free();

	return ret;
}

static int s5c73m3_power_down(void)
{
	struct regulator *regulator;
	int ret = 0;

	printk(KERN_DEBUG "%s: in\n", __func__);

	s5c73m3_gpio_request();

	/* ISP_STANDBY */
	ret = gpio_direction_output(GPIO_ISP_STANDBY, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_STANDBY");
	udelay(2);		/* 40 cycle */

	/* ISP_RESET */
	ret = gpio_direction_output(GPIO_ISP_RESET, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_ISP_RESET");

	/* CAM_AF_2.8V */
	ret = gpio_direction_output(GPIO_CAM_AF_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_AF_EN");

	/* CAM_ISP_MIPI_1.2V */
	regulator = regulator_get(NULL, "cam_isp_mipi_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_mipi_1.2v");
	udelay(10);		/* 200 cycle */

	/* CAM_ISP_SENSOR_1.8V */
	regulator = regulator_get(NULL, "cam_isp_sensor_1.8v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_sensor_1.8v");

	/* CAM_SENSOR_CORE_1.2V */
#if defined(CONFIG_MACH_C1) || defined(CONFIG_MACH_T0) || \
	defined(CONFIG_MACH_SLP_T0_LTE)
	if (system_rev >= USE_8M_CAM_SENSOR_CORE_REVISION) {
		ret = gpio_direction_output(GPIO_CAM_SENSOR_CORE_EN, 0);
		CAM_CHECK_ERR_RET(ret, "output CAM_SENSOR_CORE_EN");
		udelay(500);
	} else {
		regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			ret = regulator_force_disable(regulator);
		regulator_put(regulator);
		CAM_CHECK_ERR(ret, "disable cam_sensor_core_1.2v");
		/* delay is needed : hw request*/
		udelay(500);
	}
#else
	regulator = regulator_get(NULL, "cam_sensor_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_sensor_core_1.2v");
	/* delay is needed : hw request*/
	udelay(500);
#endif

	/* CAM_SENSOR_A2.8V */
	ret = gpio_direction_output(GPIO_CAM_IO_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_IO_EN");

	/* CAM_ISP_CORE_1.2V */
	regulator = regulator_get(NULL, "cam_isp_core_1.2v");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR(ret, "disable cam_isp_core_1.2v");

	ret = gpio_direction_output(GPIO_ISP_CORE_EN, 0);
	CAM_CHECK_ERR_RET(ret, "output GPIO_CAM_ISP_CORE_EN");
	/* delay is needed : hw request*/
	mdelay(30);

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_DOWN);
	CAM_CHECK_ERR(ret, "cfg mclk");

	s5c73m3_gpio_free();

	return ret;
}

static int s5c73m3_power(int enable)
{
	int ret = 0;

	if (enable) {
		ret = s5c73m3_power_on();

		if (unlikely(ret))
			goto error_out;
	} else
		ret = s5c73m3_power_down();

	ret = s3c_csis_power(enable);

error_out:
	return ret;
}

static int s5c73m3_get_i2c_busnum(void)
{
	return 0;
}

static const char *s5c73m3_get_clk_name(void)
{
	return "sclk_cam0";
}

static struct s5c73m3_platform_data s5c73m3_plat = {
	.default_width = 640,	/* 1920 */
	.default_height = 480,	/* 1080 */
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
	.set_vdd_core = s5c73m3_set_vdd_core,
	.is_vdd_core_set = s5c73m3_is_vdd_core_set,
	.is_isp_reset = s5c73m3_is_isp_reset,
	.power_on_off = s5c73m3_power,
};

static struct i2c_board_info s5c73m3_i2c_info = {
	I2C_BOARD_INFO("S5C73M3", 0x78 >> 1),
	.platform_data = &s5c73m3_plat,
};

static struct s3c_platform_camera s5c73m3 = {
	.id = CAMERA_CSI_C,
	.get_clk_name = s5c73m3_get_clk_name,
	.get_i2c_busnum = s5c73m3_get_i2c_busnum,
	.cam_power = s5c73m3_power,
	.type = CAM_TYPE_MIPI,
	.fmt = MIPI_CSI_YCBCR422_8BIT,
	.order422 = CAM_ORDER422_8BIT_YCBYCR,
	.info = &s5c73m3_i2c_info,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.srclk_name = "xusbxti",	/* "mout_mpll" */
	.clk_rate = 24000000,	/* 48000000 */
	.line_length = 1920,
	.width = 640,
	.height = 480,
	.window = {
		.left = 0,
		.top = 0,
		.width = 640,
		.height = 480,
	},

	.mipi_lanes = 4,
	.mipi_settle = 12,
	.mipi_align = 32,

	/* Polarity */
	.inv_pclk = 1,
	.inv_vsync = 1,
	.inv_href = 0,
	.inv_hsync = 0,
	.reset_camera = 0,
	.initialized = 0,
};
#endif


/* Interface setting */
static struct s3c_platform_fimc fimc_plat = {
	.default_cam = CAMERA_CSI_D,
	.camera = {
#if defined(CONFIG_VIDEO_SLP_S5C73M3)
		&s5c73m3,
#endif
#ifdef CONFIG_VIDEO_S5K6A3
		&s5k6a3,
#endif
#if defined(CONFIG_VIDEO_S5K6A3) && defined(CONFIG_S5K6A3_CSI_D)
		&s5k6a3_fd,
#endif
#ifdef WRITEBACK_ENABLED
		&writeback,
#endif
	},
	.hw_ver		= 0x51,
};

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
static void __set_flite_camera_config(struct exynos_platform_flite *data,
			u32 active_index, u32 max_cam)
{
	data->active_cam_index = active_index;
	data->num_clients = max_cam;
}

static void __init smdk4x12_set_camera_flite_platdata(void)
{
	int flite0_cam_index = 0;
	int flite1_cam_index = 0;
#ifdef CONFIG_VIDEO_S5K6A3
	exynos_flite1_default_data.cam[flite1_cam_index++] = &s5k6a3;
#endif
	__set_flite_camera_config(&exynos_flite0_default_data, 0,
		flite0_cam_index);
	__set_flite_camera_config(&exynos_flite1_default_data, 0,
		flite1_cam_index);
}
#endif /* CONFIG_VIDEO_EXYNOS_FIMC_LITE */
#endif /* CONFIG_VIDEO_FIMC */

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
static struct i2c_board_info __initdata test_info = {
	I2C_BOARD_INFO("testinfo", 0x0),
};

static struct s5p_fimc_isp_info isp_info[] = {
	{
		.board_info	= &test_info,
		.bus_type	= FIMC_LCD_WB,
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.flags		= FIMC_CLK_INV_VSYNC,
	},
};

static void __init midas_subdev_config(void)
{
	s3c_fimc0_default_data.isp_info[0] = &isp_info[0];
	s3c_fimc0_default_data.isp_info[0]->use_cam = true;
	s3c_fimc0_default_data.isp_info[1] = &isp_info[1];
	s3c_fimc0_default_data.isp_info[1]->use_cam = false;
	s3c_fimc0_default_data.isp_info[2] = &isp_info[1];
	s3c_fimc0_default_data.isp_info[2]->use_cam = false;
	s3c_fimc0_default_data.isp_info[3] = &isp_info[1];
	s3c_fimc0_default_data.isp_info[3]->use_cam = false;
}
#endif	/* CONFIG_VIDEO_SAMSUNG_S5P_FIMC */

static void __init pq_camera_init(void)
{
#ifdef CONFIG_VIDEO_FIMC
	s3c_fimc0_set_platdata(&fimc_plat);
	s3c_fimc1_set_platdata(&fimc_plat);
	s3c_fimc2_set_platdata(NULL);
#ifdef CONFIG_DRM_EXYNOS_FIMD_WB
	s3c_fimc3_set_platdata(&fimc_plat);
#else
	s3c_fimc3_set_platdata(NULL);
#endif
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	secmem.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#ifdef CONFIG_VIDEO_FIMC_MIPI
	s3c_csis0_set_platdata(NULL);
	s3c_csis1_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_csis1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	smdk4x12_set_camera_flite_platdata();
	s3c_set_platdata(&exynos_flite0_default_data,
		sizeof(exynos_flite0_default_data), &exynos_device_flite0);
	s3c_set_platdata(&exynos_flite1_default_data,
		sizeof(exynos_flite1_default_data), &exynos_device_flite1);
#endif
#endif /* CONFIG_VIDEO_FIMC */

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
	midas_subdev_config();

	dev_set_name(&s5p_device_fimc0.dev, "s3c-fimc.0");
	dev_set_name(&s5p_device_fimc1.dev, "s3c-fimc.1");
	dev_set_name(&s5p_device_fimc2.dev, "s3c-fimc.2");
	dev_set_name(&s5p_device_fimc3.dev, "s3c-fimc.3");

	clk_add_alias("fimc", "exynos4210-fimc.0", "fimc",
			&s5p_device_fimc0.dev);
	clk_add_alias("fimc", "exynos4210-fimc.1", "fimc",
			&s5p_device_fimc1.dev);
	clk_add_alias("fimc", "exynos4210-fimc.2", "fimc",
			&s5p_device_fimc2.dev);
	clk_add_alias("fimc", "exynos4210-fimc.3", "fimc",
			&s5p_device_fimc3.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.0", "sclk_fimc",
			&s5p_device_fimc0.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.1", "sclk_fimc",
			&s5p_device_fimc1.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.2", "sclk_fimc",
			&s5p_device_fimc2.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.3", "sclk_fimc",
			&s5p_device_fimc3.dev);

	s3c_fimc_setname(0, "exynos4210-fimc");
	s3c_fimc_setname(1, "exynos4210-fimc");
	s3c_fimc_setname(2, "exynos4210-fimc");
	s3c_fimc_setname(3, "exynos4210-fimc");

	s3c_set_platdata(&s3c_fimc0_default_data,
			 sizeof(s3c_fimc0_default_data), &s5p_device_fimc0);
	s3c_set_platdata(&s3c_fimc1_default_data,
			 sizeof(s3c_fimc1_default_data), &s5p_device_fimc1);
	s3c_set_platdata(&s3c_fimc2_default_data,
			 sizeof(s3c_fimc2_default_data), &s5p_device_fimc2);
	s3c_set_platdata(&s3c_fimc3_default_data,
			 sizeof(s3c_fimc3_default_data), &s5p_device_fimc3);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif /* CONFIG_VIDEO_S5P_FIMC */

}



#ifdef CONFIG_EXYNOS4_DEV_DWMCI
#define DIV_FSYS3	(S5P_VA_CMU + 0x0C54C)
static void exynos_dwmci_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS4_GPK0(0); gpio < EXYNOS4_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	width = (1 << width);

	switch (width) {
	case 8:
		for (gpio = EXYNOS4_GPK1(3); gpio <= EXYNOS4_GPK1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(4));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
	case 4:
		for (gpio = EXYNOS4_GPK0(3); gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);

		/* Workaround
		 * NOTE : In future, should be modified bootloader
		 * Set this value when 1-bit buswidth(it's initial time)*/
		__raw_writel(0x1, DIV_FSYS3);
	default:
		break;
	}
}

/*
 * block setting of dwmci
 * max_segs = PAGE_SIZE / size of IDMAC desc,
 * max_blk_size = 512,
 * max_blk_count = 65536,
 * max_seg_size = PAGE_SIZE,
 * max_req_size = max_seg_size * max_blk_count
 **/
static struct block_settings exynos_dwmci_blk_setting = {
	.max_segs		= 0x1000,
	.max_blk_size		= 0x200,
	.max_blk_count		= 0x10000,
	.max_seg_size		= 0x1000,
	.max_req_size		= 0x1000 * 0x10000,
};

static struct dw_mci_board exynos_dwmci_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 100 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
				MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
	.caps2			= MMC_CAP2_PACKED_CMD,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci_cfg_gpio,
	.blk_settings		= &exynos_dwmci_blk_setting,
	.buf_size		= PAGE_SIZE << 4,
};
#else
static struct s3c_mshci_platdata exynos4_mshc_pdata __initdata = {
	.cd_type                = S3C_MSHCI_CD_PERMANENT,
	.fifo_depth		= 0x80,
	.max_width              = 8,
	.host_caps              = MMC_CAP_8_BIT_DATA | MMC_CAP_1_8V_DDR |
				  MMC_CAP_UHS_DDR50 | MMC_CAP_CMD23,
	.host_caps2		= MMC_CAP2_PACKED_CMD,
};
#endif

static struct s3c_sdhci_platdata slp_midas_hsmmc2_pdata __initdata = {
	.cd_type                = S3C_SDHCI_CD_GPIO,
	.ext_cd_gpio            = EXYNOS4_GPX3(4),
	.ext_cd_gpio_invert	= true,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
	.max_width		= 4,
	.host_caps		= MMC_CAP_4_BIT_DATA,
	.vmmc_name		= "vtf_2.8v",
};

#ifdef CONFIG_LEDS_AAT1290A
static int aat1290a_initGpio(void)
{
	int err;

	err = gpio_request(GPIO_CAM_SW_EN, "CAM_SW_EN");
	if (err) {
		printk(KERN_ERR "failed to request CAM_SW_EN\n");
		return -EPERM;
	}
	gpio_direction_output(GPIO_CAM_SW_EN, 1);

	return 0;
}

static void aat1290a_switch(int enable)
{
	gpio_set_value(GPIO_CAM_SW_EN, enable);
}

static int aat1290a_setGpio(void)
{
	int err;

	err = gpio_request(GPIO_TORCH_EN, "TORCH_EN");
	if (err) {
		printk(KERN_ERR "failed to request TORCH_EN\n");
		return -EPERM;
	}
	gpio_direction_output(GPIO_TORCH_EN, 0);
	err = gpio_request(GPIO_TORCH_SET, "TORCH_SET");
	if (err) {
		printk(KERN_ERR "failed to request TORCH_SET\n");
		gpio_free(GPIO_TORCH_EN);
		return -EPERM;
	}
	gpio_direction_output(GPIO_TORCH_SET, 0);

	return 0;
}

static int aat1290a_freeGpio(void)
{
	gpio_free(GPIO_TORCH_EN);
	gpio_free(GPIO_TORCH_SET);

	return 0;
}

static void aat1290a_torch_en(int onoff)
{
	gpio_set_value(GPIO_TORCH_EN, onoff);
}

static void aat1290a_torch_set(int onoff)
{
	gpio_set_value(GPIO_TORCH_SET, onoff);
}

static struct aat1290a_led_platform_data aat1290a_led_data = {
	.brightness = 0,
	.status	= STATUS_UNAVAILABLE,
	.switch_sel = aat1290a_switch,
	.initGpio = aat1290a_initGpio,
	.setGpio = aat1290a_setGpio,
	.freeGpio = aat1290a_freeGpio,
	.torch_en = aat1290a_torch_en,
	.torch_set = aat1290a_torch_set,
};

static struct platform_device s3c_device_aat1290a_led = {
	.name	= "aat1290a-led",
	.id	= -1,
	.dev	= {
		.platform_data	= &aat1290a_led_data,
	},
};
#endif

static DEFINE_MUTEX(notify_lock);

/* FIXME: For coexistence of both slp-pq and redwood board. Need to fix */
#define DEFINE_MMC_CARD_NOTIFIER(num) \
void (*hsmmc##num##_notify_func)(struct platform_device *, int state); \
static int ext_cd_init_hsmmc##num(void (*notify_func)( \
			struct platform_device *, int state)) \
{ \
	mutex_lock(&notify_lock); \
	WARN_ON(hsmmc##num##_notify_func); \
	hsmmc##num##_notify_func = notify_func; \
	mutex_unlock(&notify_lock); \
	return 0; \
} \
static int ext_cd_cleanup_hsmmc##num(void (*notify_func)( \
			struct platform_device *, int state)) \
{ \
	mutex_lock(&notify_lock); \
	WARN_ON(hsmmc##num##_notify_func != notify_func); \
	hsmmc##num##_notify_func = NULL; \
	mutex_unlock(&notify_lock); \
	return 0; \
}

DEFINE_MMC_CARD_NOTIFIER(3)

/*
 * call this when you need sd stack to recognize insertion or removal of card
 * that can't be told by SDHCI regs
 */

void mmc_force_presence_change(struct platform_device *pdev)
{
	void (*notify_func)(struct platform_device *, int state) = NULL;
	mutex_lock(&notify_lock);
	if (pdev == &s3c_device_hsmmc3)
		notify_func = hsmmc3_notify_func;

	if (notify_func)
		notify_func(pdev, 1);
	else
		pr_warn("%s: called for device with no notifier\n", __func__);
	mutex_unlock(&notify_lock);
}
EXPORT_SYMBOL_GPL(mmc_force_presence_change);

static struct s3c_sdhci_platdata slp_midas_hsmmc3_pdata __initdata = {
/* new code for brm4334 */
	.cd_type	= S3C_SDHCI_CD_EXTERNAL,
	.clk_type	= S3C_SDHCI_CLK_DIV_EXTERNAL,
	.pm_flags	= S3C_SDHCI_PM_IGNORE_SUSPEND_RESUME,
	.ext_cd_init	= ext_cd_init_hsmmc3,
	.ext_cd_cleanup	= ext_cd_cleanup_hsmmc3,
};

#ifdef CONFIG_DRM_EXYNOS
static struct resource exynos_drm_resource[] = {
	[0] = {
		.start = IRQ_FIMD0_VSYNC,
		.end   = IRQ_FIMD0_VSYNC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device exynos_drm_device = {
	.name	= "exynos-drm",
	.id	= -1,
	.num_resources	  = ARRAY_SIZE(exynos_drm_resource),
	.resource	  = exynos_drm_resource,
	.dev	= {
		.dma_mask = &exynos_drm_device.dev.coherent_dma_mask,
		.coherent_dma_mask = 0xffffffffUL,
	}
};
#endif

enum fixed_regulator_id {
	FIXED_REG_ID_LCD = 0,
	FIXED_REG_ID_HDMI = 1,
};

#ifdef CONFIG_DRM_EXYNOS_FIMD
static struct exynos_drm_fimd_pdata drm_fimd_pdata = {
	.panel = {
		.timing	= {
			.xres		= 720,
			.yres		= 1280,
			.hsync_len	= 5,
			.left_margin	= 10,
			.right_margin	= 10,
			.vsync_len	= 2,
			.upper_margin	= 1,
			.lower_margin	= 13,
			.refresh	= 60,
		},
		.width_mm	= 58,
		.height_mm	= 103,
	},
	.vidcon0		= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1		= VIDCON1_INV_VCLK,
	.default_win		= 3,
	.bpp			= 32,
};

#ifdef CONFIG_MDNIE_SUPPORT
static struct resource exynos4_fimd_lite_resource[] = {
	[0] = {
		.start	= EXYNOS4_PA_LCD_LITE0,
		.end	= EXYNOS4_PA_LCD_LITE0 + S5P_SZ_LCD_LITE0 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_LCD_LITE0,
		.end	= IRQ_LCD_LITE0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource exynos4_mdnie_resource[] = {
	[0] = {
		.start	= EXYNOS4_PA_MDNIE0,
		.end	= EXYNOS4_PA_MDNIE0 + S5P_SZ_MDNIE0 - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct mdnie_platform_data exynos4_mdnie_pdata = {
	.width			= 720,
	.height			= 1280,
	.model			= PQ_mdnie,
};

static struct s5p_fimd_ext_device exynos4_fimd_lite_device = {
	.name			= "fimd_lite",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(exynos4_fimd_lite_resource),
	.resource		= exynos4_fimd_lite_resource,
	.dev			= {
		.platform_data	= &drm_fimd_pdata,
	},
};

static struct s5p_fimd_ext_device exynos4_mdnie_device = {
	.name			= "mdnie",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(exynos4_mdnie_resource),
	.resource		= exynos4_mdnie_resource,
	.dev			= {
		.platform_data	= &exynos4_mdnie_pdata,
	},
};

/* FIXME:!! why init at this point ? */
static int exynos4_common_setup_clock(const char *sclk_name,
	const char *pclk_name, unsigned long rate, unsigned int rate_set)
{
	struct clk *sclk = NULL;
	struct clk *pclk = NULL;

	sclk = clk_get(NULL, sclk_name);
	if (IS_ERR(sclk)) {
		printk(KERN_ERR "failed to get %s clock.\n", sclk_name);
		goto err_clk;
	}

	pclk = clk_get(NULL, pclk_name);
	if (IS_ERR(pclk)) {
		printk(KERN_ERR "failed to get %s clock.\n", pclk_name);
		goto err_clk;
	}

	clk_set_parent(sclk, pclk);

	printk(KERN_INFO "set parent clock of %s to %s\n", sclk_name,
			pclk_name);
	if (!rate_set)
		goto set_end;

	if (!rate)
		rate = 200 * MHZ;

	clk_set_rate(sclk, rate);

set_end:
	clk_put(sclk);
	clk_put(pclk);

	return 0;

err_clk:
	clk_put(sclk);
	clk_put(pclk);

	return -EINVAL;

}
#endif

static struct regulator_consumer_supply lcd_supplies[] = {
	REGULATOR_SUPPLY("VDD3", "s6e8aa0"),
};

static struct regulator_init_data lcd_fixed_reg_initdata = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(lcd_supplies),
	.consumer_supplies = lcd_supplies,
};

static struct fixed_voltage_config lcd_config = {
	.init_data = &lcd_fixed_reg_initdata,
	.microvolts = 2200000,
	.supply_name = "VDD3",
	.gpio = GPIO_LCD_22V_EN_00,
	.enable_high = 1,
	.enabled_at_boot = 1,
};

static struct platform_device lcd_fixed_reg_device = {
	.name = "reg-fixed-voltage",
	.id = FIXED_REG_ID_LCD,
	.dev = {
		.platform_data = &lcd_config,
	},
};

static void reset_lcd_low(void)
{
	int reset_gpio = EXYNOS4_GPY4(5);

	gpio_request(reset_gpio, "MLCD_RST");
	gpio_direction_output(reset_gpio, 0);
	gpio_free(reset_gpio);
}

static int reset_lcd(struct lcd_device *ld)
{
	int reset_gpio = EXYNOS4_GPY4(5);

	gpio_request(reset_gpio, "MLCD_RST");
	gpio_direction_output(reset_gpio, 1);
	mdelay(10);
	gpio_direction_output(reset_gpio, 0);
	mdelay(10);
	gpio_direction_output(reset_gpio, 1);
	gpio_free(reset_gpio);
	dev_info(&ld->dev, "reset completed.\n");

	return 0;
}

static struct lcd_property s6e8aa0_property = {
	.flip = LCD_PROPERTY_FLIP_NONE,
	.reset_low = reset_lcd_low,
};

static struct lcd_platform_data s6e8aa0_pdata = {
	.reset			= reset_lcd,
	.reset_delay		= 25,
	.power_off_delay	= 120,
	.power_on_delay		= 120,
	.lcd_enabled		= 1,
	.pdata	= &s6e8aa0_property,
};

static int get_lcd_esd_level(void)
{
	int lcd_det_gpio = EXYNOS4_GPF3(0);
	int ret;

	gpio_request(lcd_det_gpio, "LCD_DET");
	ret = gpio_get_value(lcd_det_gpio);
	gpio_free(lcd_det_gpio);

	return ret;
}

static void lcd_esd_gpio_suspend(void)
{
	int lcd_det_gpio = EXYNOS4_GPF3(0);

	gpio_request(lcd_det_gpio, "LCD_DET");
	s3c_gpio_cfgpin(lcd_det_gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(lcd_det_gpio, S3C_GPIO_PULL_NONE);
	gpio_direction_output(GPIO_OLED_DET, GPIO_LEVEL_LOW);
	gpio_free(lcd_det_gpio);
}

static void lcd_esd_gpio_resume(void)
{
	int lcd_det_gpio = EXYNOS4_GPF3(0);

	gpio_request(lcd_det_gpio, "LCD_DET");
	s3c_gpio_cfgpin(lcd_det_gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(lcd_det_gpio, S3C_GPIO_PULL_UP);
	gpio_free(lcd_det_gpio);
}

static struct lcd_property s6d6aa1_property = {
	.flip = LCD_PROPERTY_FLIP_VERTICAL |
		LCD_PROPERTY_FLIP_HORIZONTAL,
	.esd_level = 1,
	.get_esd_level = get_lcd_esd_level,
	.esd_gpio_suspend = lcd_esd_gpio_suspend,
	.esd_gpio_resume = lcd_esd_gpio_resume,
	.reset_low = reset_lcd_low,
};

static struct lcd_platform_data s6d6aa1_pdata = {
	.reset			= reset_lcd,
	.reset_delay		= 50,
	.power_off_delay	= 130,
	.power_on_delay		= 10,
	.lcd_enabled		= 1,
	.pdata	= &s6d6aa1_property,
};

static void lcd_cfg_gpio(void)
{
	int reg;
	int lcd_enable_gpio = GPIO_LCD_22V_EN_00;
	int reset_gpio = EXYNOS4_GPF2(1);
	int lcd_det_gpio = EXYNOS4_GPF3(0);
	int lcd_enable_gpio_redwood = EXYNOS4212_GPM0(0);

	if (board_is_m0() && hwrevision(M0_PROXIMA_REV0_1)) {
		/* LCD_EN */
		gpio_request(lcd_enable_gpio, "LCD_EN");
		s3c_gpio_cfgpin(lcd_enable_gpio, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(lcd_enable_gpio, S3C_GPIO_PULL_NONE);
		gpio_free(lcd_enable_gpio);
	}  else if (board_is_redwood()) {
		/* MLCD_RST */
		gpio_request(reset_gpio, "MLCD_RST");
		s3c_gpio_cfgpin(reset_gpio, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(reset_gpio, S3C_GPIO_PULL_NONE);
		gpio_free(reset_gpio);

		/* LCD_EN */
		gpio_request(lcd_enable_gpio_redwood, "LCD_EN");
		s3c_gpio_cfgpin(lcd_enable_gpio_redwood, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(lcd_enable_gpio_redwood, S3C_GPIO_PULL_NONE);
		gpio_free(lcd_enable_gpio_redwood);

		/* LCD_DET */
		gpio_request(lcd_det_gpio, "LCD_DET");
		s3c_gpio_cfgpin(lcd_det_gpio, S3C_GPIO_SFN(0xf));
		s3c_gpio_setpull(lcd_det_gpio, S3C_GPIO_PULL_UP);
		s5p_register_gpio_interrupt(lcd_det_gpio);
		s6d6aa1_property.esd_irq = gpio_to_irq(lcd_det_gpio);
		gpio_free(lcd_det_gpio);
	}

	reg = __raw_readl(S3C_VA_SYS + 0x210);
	reg |= 1 << 1;
	__raw_writel(reg, S3C_VA_SYS + 0x210);

	return;
}

#ifdef CONFIG_S5P_MIPI_DSI2
static struct mipi_dsim_config dsim_config = {
	.e_interface		= DSIM_VIDEO,
	.e_virtual_ch		= DSIM_VIRTUAL_CH_0,
	.e_pixel_format		= DSIM_24BPP_888,
	.e_burst_mode		= DSIM_BURST_SYNC_EVENT,
	.e_no_data_lane		= DSIM_DATA_LANE_4,
	.e_byte_clk		= DSIM_PLL_OUT_DIV8,
	.cmd_allow		= 0xf,

	/*
	 * ===========================================
	 * |    P    |    M    |    S    |    MHz    |
	 * -------------------------------------------
	 * |    3    |   100   |    3    |    100    |
	 * |    3    |   100   |    2    |    200    |
	 * |    3    |    63   |    1    |    252    |
	 * |    4    |   100   |    1    |    300    |
	 * |    4    |   110   |    1    |    330    |
	 * |   12    |   350   |    1    |    350    |
	 * |    3    |   100   |    1    |    400    |
	 * |    4    |   150   |    1    |    450    |
	 * |    3    |   120   |    1    |    480    |
	 * |   12    |   250   |    0    |    500    |
	 * |    4    |   100   |    0    |    600    |
	 * |    3    |    81   |    0    |    648    |
	 * |    3    |    88   |    0    |    704    |
	 * |    3    |    90   |    0    |    720    |
	 * |    3    |   100   |    0    |    800    |
	 * |   12    |   425   |    0    |    850    |
	 * |    4    |   150   |    0    |    900    |
	 * |   12    |   475   |    0    |    950    |
	 * |    6    |   250   |    0    |   1000    |
	 * -------------------------------------------
	 */

	.p			= 12,
	.m			= 250,
	.s			= 0,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time	= 500,

	/* escape clk : 10MHz */
	.esc_clk		= 10 * 1000000,

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0x7ff,
	/* bta timeout 0 ~ 0xff */
	.bta_timeout		= 0xff,
	/* lp rx timeout 0 ~ 0xffff */
	.rx_timeout		= 0xffff,
};

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	/* already enabled at boot loader. FIXME!!! */
	.enabled		= true,
	.phy_enable		= s5p_dsim_phy_enable,
	.dsim_config		= &dsim_config,
};

static struct mipi_dsim_lcd_device mipi_lcd_device = {
	.name			= "s6e8aa0",
	.id			= -1,
	.bus_id			= 0,

	.platform_data		= (void *)&s6e8aa0_pdata,
};
#endif

static void __init midas_fb_init(void)
{
#ifdef CONFIG_S5P_MIPI_DSI2
	struct s5p_platform_mipi_dsim *dsim_pdata;

	dsim_pdata = (struct s5p_platform_mipi_dsim *)&dsim_platform_data;
	strcpy(dsim_pdata->lcd_panel_name, "s6e8aa0");
	if (board_is_redwood()) {
		drm_fimd_pdata.panel.timing.hsync_len = 3;
		drm_fimd_pdata.panel.timing.left_margin = 60;
		drm_fimd_pdata.panel.timing.right_margin = 60;
		drm_fimd_pdata.panel.timing.vsync_len = 2;
		drm_fimd_pdata.panel.timing.upper_margin = 2;
		drm_fimd_pdata.panel.timing.lower_margin = 36;

		mipi_lcd_device.platform_data = (void *)&s6d6aa1_pdata;
		strcpy(mipi_lcd_device.name, "s6d6aa1");
		strcpy(dsim_pdata->lcd_panel_name, "s6d6aa1");
	}

	dsim_pdata->lcd_panel_info = (void *)&drm_fimd_pdata.panel.timing;

	s5p_mipi_dsi_register_lcd_device(&mipi_lcd_device);
	if (board_is_m0() && hwrevision(M0_PROXIMA_REV0_1))
		platform_device_register(&lcd_fixed_reg_device);
#ifdef CONFIG_MDNIE_SUPPORT
	s5p_fimd_ext_device_register(&exynos4_mdnie_device);
	s5p_fimd_ext_device_register(&exynos4_fimd_lite_device);
	exynos4_common_setup_clock("sclk_mdnie", "mout_mpll_user",
				400 * MHZ, 1);
#endif
	s5p_device_mipi_dsim0.dev.platform_data = (void *)&dsim_platform_data;
	platform_device_register(&s5p_device_mipi_dsim0);
#endif

	s5p_device_fimd0.dev.platform_data = &drm_fimd_pdata;
	lcd_cfg_gpio();
}

static unsigned long fbmem_start;
static int __init early_fbmem(char *p)
{
	char *endp;
	unsigned long size;

	if (!p)
		return -EINVAL;

	size = memparse(p, &endp);
	if (*endp == '@')
		fbmem_start = memparse(endp + 1, &endp);

	return endp > p ? 0 : -EINVAL;
}
early_param("fbmem", early_fbmem);
#endif

#ifdef CONFIG_DRM_EXYNOS_HDMI
/* I2C HDMIPHY */
static struct s3c2410_platform_i2c hdmiphy_i2c_data __initdata = {
	.bus_num	= 8,
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 100*1000,
	.sda_delay	= 100,
};

static struct i2c_board_info i2c_hdmiphy_devs[] __initdata = {
	{
		/* hdmiphy */
		I2C_BOARD_INFO("s5p_hdmiphy", (0x70 >> 1)),
	},
};

static struct exynos_drm_hdmi_pdata drm_hdmi_pdata = {
	.cfg_hpd	= s5p_hdmi_cfg_hpd,
	.get_hpd	= s5p_hdmi_get_hpd,
#ifdef CONFIG_EXTCON
	.extcon_name = "max77693-muic",
#endif
};

static struct exynos_drm_common_hdmi_pd drm_common_hdmi_pdata = {
	.hdmi_dev	= &s5p_device_hdmi.dev,
	.mixer_dev	= &s5p_device_mixer.dev,
};

static struct platform_device exynos_drm_hdmi_device = {
	.name	= "exynos-drm-hdmi",
	.dev	= {
		.platform_data = &drm_common_hdmi_pdata,
	},
};

static void midas_tv_init(void)
{
	/* HDMI PHY */
	s5p_i2c_hdmiphy_set_platdata(&hdmiphy_i2c_data);
	i2c_register_board_info(8, i2c_hdmiphy_devs,
				ARRAY_SIZE(i2c_hdmiphy_devs));

	gpio_request(GPIO_HDMI_HPD, "HDMI_HPD");
	gpio_direction_input(GPIO_HDMI_HPD);
	s3c_gpio_cfgpin(GPIO_HDMI_HPD, S3C_GPIO_SFN(0x3));
	s3c_gpio_setpull(GPIO_HDMI_HPD, S3C_GPIO_PULL_DOWN);

#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_hdmi.dev.parent = &exynos4_device_pd[PD_TV].dev;
	s5p_device_mixer.dev.parent = &exynos4_device_pd[PD_TV].dev;
#endif
	s5p_device_hdmi.dev.platform_data = &drm_hdmi_pdata;
}

/* FIXME:!! must move to midas-mhl.c */
#ifndef CONFIG_HDMI_HPD
/* Dummy function */
void mhl_hpd_handler(bool onoff)
{
	printk(KERN_INFO "hpd(%d)\n", onoff);
}
EXPORT_SYMBOL(mhl_hpd_handler);
#endif
#endif

static struct platform_device exynos_drm_vidi_device = {
	.name	= "exynos-drm-vidi",
};

#ifdef CONFIG_DRM_EXYNOS_IPP
static struct platform_device exynos_drm_ipp_device = {
	.name	= "exynos-drm-ipp",
};
#endif

#ifdef CONFIG_DRM_EXYNOS_FIMC
static struct resource exynos_drm_fimc2_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC2,
		.end	= S5P_PA_FIMC2 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC2,
		.end	= IRQ_FIMC2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource exynos_drm_fimc3_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC3,
		.end	= S5P_PA_FIMC3 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC3,
		.end	= IRQ_FIMC3,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 exynos_drm_fimc_dma_mask = DMA_BIT_MASK(32);

static struct exynos_drm_fimc_pdata drm_fimc_pdata = {
	.pol	= {
		/* warnning: find vidcon1 in drm_fimd_pdata
			this value related with fimd for wb */
		.inv_pclk = 0,
		.inv_vsync = 0,
		.inv_href = 0,
		.inv_hsync = 0,
	},
	.ver = FIMC_EXYNOS_4412,
};

static struct platform_device exynos_drm_fimc2_device = {
	.name	= "exynos-drm-fimc",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(exynos_drm_fimc2_resource),
	.resource	= exynos_drm_fimc2_resource,
	.dev		= {
		.platform_data = &drm_fimc_pdata,
		.dma_mask	= &exynos_drm_fimc_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct platform_device exynos_drm_fimc3_device = {
	.name	= "exynos-drm-fimc",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(exynos_drm_fimc3_resource),
	.resource	= exynos_drm_fimc3_resource,
	.dev		= {
		.platform_data = &drm_fimc_pdata,
		.dma_mask	= &exynos_drm_fimc_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static int midas_fimc_init(void)
{
	printk(KERN_INFO "%s\n", __func__);

	exynos_drm_fimc2_device.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	exynos_drm_fimc3_device.dev.parent = &exynos4_device_pd[PD_CAM].dev;

	return 0;
}
#endif

static struct i2c_board_info i2c_devs0[] __initdata = {
	/*
	 * GPD1(0, 1) / XI2C0SDA/SCL
	 * PQ_LTE: 8M_CAM, PQ(proxima): NC
	 */
};

static int lsm330dlc_get_position(void)
{
	int position = 0;

#if defined(CONFIG_MACH_SLP_PQ)
	if (system_rev == 3 || system_rev == 0)
		position = 6; /* top/lower-right */
	else
		position = 2; /* bottom/lower-right */
#elif defined(CONFIG_MACH_SLP_PQ_LTE)
	position = 7; /* top/lower-left */
#else /* Common */
	position = 6; /* top/lower-right */
#endif
	return position;
}

static struct accel_platform_data lsm330dlc_accel_pdata = {
	.accel_get_position = lsm330dlc_get_position,
	.axis_adjust = true,
};

static struct gyro_platform_data lsm330dlc_gyro_pdata = {
	.gyro_get_position = lsm330dlc_get_position,
	.axis_adjust = true,
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	/* PQ_LTE/PQ both use GSENSE_SCL/SDA */
	{
		I2C_BOARD_INFO("lsm330dlc_accel", (0x32 >> 1)),
		.platform_data = &lsm330dlc_accel_pdata,
	},
	{
		I2C_BOARD_INFO("lsm330dlc_gyro", (0xD6 >> 1)),
		.platform_data = &lsm330dlc_gyro_pdata,
	},
};

static void lsm331dlc_gpio_init(void)
{
	int ret = gpio_request(GPIO_GYRO_INT, "lsm330dlc_gyro_irq");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio lsm330dlc_gyro_irq\n");

	ret = gpio_request(GPIO_GYRO_DE, "lsm330dlc_gyro_data_enable");

	if (ret)
		printk(KERN_ERR "Failed to request gpio lsm330dlc_gyro_data_enable\n");

	ret = gpio_request(GPIO_ACC_INT, "lsm330dlc_accel_irq");

	if (ret)
		printk(KERN_ERR "Failed to request gpio lsm330dlc_accel_irq\n");

	/* Accelerometer sensor interrupt pin initialization */
	s3c_gpio_cfgpin(GPIO_ACC_INT, S3C_GPIO_INPUT);
	gpio_set_value(GPIO_ACC_INT, 2);
	s3c_gpio_setpull(GPIO_ACC_INT, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(GPIO_ACC_INT, S5P_GPIO_DRVSTR_LV1);
	i2c_devs1[0].irq = gpio_to_irq(GPIO_ACC_INT);

	/* Gyro sensor interrupt pin initialization */
	s3c_gpio_cfgpin(GPIO_GYRO_INT, S3C_GPIO_INPUT);
	gpio_set_value(GPIO_GYRO_INT, 2);
	s3c_gpio_setpull(GPIO_GYRO_INT, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(GPIO_GYRO_INT, S5P_GPIO_DRVSTR_LV1);
	i2c_devs1[1].irq = gpio_to_irq(GPIO_GYRO_INT);

	/* Gyro sensor data enable pin initialization */
	s3c_gpio_cfgpin(GPIO_GYRO_DE, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_GYRO_DE, 0);
	s3c_gpio_setpull(GPIO_GYRO_DE, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_drvstr(GPIO_GYRO_DE, S5P_GPIO_DRVSTR_LV1);
}

#if defined(CONFIG_VIBETONZ)
static struct max77693_haptic_platform_data max77693_haptic_pdata = {
	.max_timeout = 10000,
	.duty = 37641,
	.period = 38022,
	.reg2 = MOTOR_LRA | EXT_PWM | DIVIDER_128,
	.init_hw = NULL,
	.motor_en = NULL,
	.pwm_id = 0,
	.regulator_name = "vmotor",
};
#elif defined(CONFIG_HAPTIC_MAX77693)
static struct max77693_haptic_platform_data max77693_haptic_pdata = {
	.name = "motor",
	.pwm_duty = 37641,
	.pwm_period = 38022,
	.pwm_id = 0,
	.regulator_name = "vmotor",
};
#endif

#ifdef CONFIG_LEDS_MAX77693
static struct max77693_led_platform_data max77693_led_pdata = {
	.num_leds = 4,

	.leds[0].name = "leds-sec1",
	.leds[0].id = MAX77693_FLASH_LED_1,
	.leds[0].timer = MAX77693_FLASH_TIME_500MS,
	.leds[0].timer_mode = MAX77693_TIMER_MODE_MAX_TIMER,
	.leds[0].cntrl_mode = MAX77693_LED_CTRL_BY_FLASHSTB,
	.leds[0].brightness = 0x1F,

	.leds[1].name = "leds-sec2",
	.leds[1].id = MAX77693_FLASH_LED_2,
	.leds[1].timer = MAX77693_FLASH_TIME_500MS,
	.leds[1].timer_mode = MAX77693_TIMER_MODE_MAX_TIMER,
	.leds[1].cntrl_mode = MAX77693_LED_CTRL_BY_FLASHSTB,
	.leds[1].brightness = 0x1F,

	.leds[2].name = "torch-sec1",
	.leds[2].id = MAX77693_TORCH_LED_1,
	.leds[2].cntrl_mode = MAX77693_LED_CTRL_BY_FLASHSTB,
	.leds[2].brightness = 0x0F,

	.leds[3].name = "torch-sec2",
	.leds[3].id = MAX77693_TORCH_LED_2,
	.leds[3].cntrl_mode = MAX77693_LED_CTRL_BY_I2C,
	.leds[3].brightness = 0x0F,

};
#endif

static struct max77693_charger_reg_data max77693_charger_regs[] = {
	{
		/*
		 * charger setting unlock
		 */
		.addr = MAX77693_CHG_REG_CHG_CNFG_06,
		.data = 0x3 << 2,
	}, {
		/*
		 * fast-charge timer : 10hr
		 * charger restart threshold : disabled
		 * low-battery prequalification mode : enabled
		 */
		.addr = MAX77693_CHG_REG_CHG_CNFG_01,
		.data = (0x4 << 0) | (0x3 << 4),
	}, {
		/*
		 * CHGIN output current limit in OTG mode : 900mA
		 * fast-charge current : 466mA
		 */
		.addr = MAX77693_CHG_REG_CHG_CNFG_02,
		.data = (1 << 7) | 0xf,
	}, {
		/*
		 * TOP off timer setting : 0min
		 * TOP off current threshold : 100mA
		 */
		.addr = MAX77693_CHG_REG_CHG_CNFG_03,
		.data = 0x0,
	}, {
		/*
		* minimum system regulation voltage : 3.6V
		* primary charge termination voltage : 4.2V
		*/
		.addr = MAX77693_CHG_REG_CHG_CNFG_04,
		.data = 0xd6,
	}, {
		/*
		 * maximum input current limit : 600mA
		 */
		.addr = MAX77693_CHG_REG_CHG_CNFG_09,
		.data = 0x1e,
	}, {
		/*
		 * VBYPSET 5V for USB HOST
		 */
		.addr = MAX77693_CHG_REG_CHG_CNFG_11,
		.data = 0x50,
	},
};

static struct max77693_charger_platform_data max77693_charger_pdata = {
	.init_data = max77693_charger_regs,
	.num_init_data = ARRAY_SIZE(max77693_charger_regs),
};

static void max77693_change_top_off_vol(void)
{
	int i = 0;

	/*
	* minimum system regulation voltage : 3.6V
	* primary charge termination voltage : 4.35V
	*/
	for (i = 0; i < max77693_charger_pdata.num_init_data; i++) {
		if (max77693_charger_pdata.init_data[i].addr ==
				MAX77693_CHG_REG_CHG_CNFG_04)
			max77693_charger_pdata.init_data[i].data = 0xdd;
	}

	return ;
}

static struct max77693_platform_data midas_max77693_info = {
	.irq_base	= IRQ_BOARD_IFIC_START,
	.irq_gpio	= GPIO_IF_PMIC_IRQ,
	.wakeup		= 1,
	.muic = &max77693_muic,
	.regulators = &max77693_regulators,
	.num_regulators = MAX77693_REG_MAX,
#if defined(CONFIG_VIBETONZ) || defined(CONFIG_HAPTIC_MAX77693)
	.haptic_data = &max77693_haptic_pdata,
#endif
#ifdef CONFIG_LEDS_MAX77693
	.led_data = &max77693_led_pdata,
#endif
	.charger_data = &max77693_charger_pdata,
};


static struct max77693_platform_data midas_max77693_no_led_info = {
	.irq_base	= IRQ_BOARD_IFIC_START,
	.irq_gpio	= GPIO_IF_PMIC_IRQ,
	.wakeup		= 1,
	.muic = &max77693_muic,
	.regulators = &max77693_regulators,
	.num_regulators = MAX77693_REG_MAX,
#if defined(CONFIG_VIBETONZ) || defined(CONFIG_HAPTIC_MAX77693)
	.haptic_data = &max77693_haptic_pdata,
#endif
	.charger_data = &max77693_charger_pdata,
};

/* I2C GPIO: PQ/PQ_LTE use GPM2[0,1] for MAX77693 */
static struct i2c_gpio_platform_data gpio_i2c_if_pmic = {
	/* PQ/PQLTE use GPF1(4, 5) */
	.sda_pin = GPIO_IF_PMIC_SDA,
	.scl_pin = GPIO_IF_PMIC_SCL,
};

static struct platform_device device_i2c_if_pmic = {
	.name = "i2c-gpio",
	.id = I2C_IF_PMIC,
	.dev.platform_data = &gpio_i2c_if_pmic,
};

static struct i2c_board_info i2c_devs_if_pmic[] __initdata = {
	{
		I2C_BOARD_INFO("max77693", (0xCC >> 1)),
		.platform_data = &midas_max77693_info,
	},
};

static struct i2c_board_info i2c_devs_if_pmic_no_led[] __initdata = {
	{
		I2C_BOARD_INFO("max77693", (0xCC >> 1)),
		.platform_data = &midas_max77693_no_led_info,
	},
};


/* Both PQ/PQ_LTE use I2C7 (XPWMTOUT_2/3) for MAX77686 */
static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("max77686", (0x12 >> 1)),
		.platform_data	= &exynos4_max77686_info,
	}
};

#ifdef CONFIG_USB_EHCI_S5P
static struct s5p_ehci_platdata smdk4212_ehci_pdata;

static void __init smdk4212_ehci_init(void)
{
	struct s5p_ehci_platdata *pdata = &smdk4212_ehci_pdata;

	s5p_ehci_set_platdata(pdata);
}
#endif

#ifdef CONFIG_USB_OHCI_S5P
static struct s5p_ohci_platdata smdk4212_ohci_pdata;

static void __init smdk4212_ohci_init(void)
{
	struct s5p_ohci_platdata *pdata = &smdk4212_ohci_pdata;

	s5p_ohci_set_platdata(pdata);
}
#endif

/* USB GADGET */
#ifdef CONFIG_USB_GADGET
static struct s5p_usbgadget_platdata smdk4212_usbgadget_pdata;

static void __init smdk4212_usbgadget_init(void)
{
	struct s5p_usbgadget_platdata *pdata = &smdk4212_usbgadget_pdata;

	s5p_usbgadget_set_platdata(pdata);
}
#endif

#ifdef CONFIG_USB_G_SLP
#include <linux/usb/slp_multi.h>
static struct slp_multi_func_data midas_slp_multi_funcs[] = {
	{
		.name = "mtp",
		.usb_config_id = USB_CONFIGURATION_DUAL,
	}, {
		.name = "acm",
		.usb_config_id = USB_CONFIGURATION_2,
	}, {
		.name = "sdb",
		.usb_config_id = USB_CONFIGURATION_2,
	}, {
		.name = "mass_storage",
		.usb_config_id = USB_CONFIGURATION_1,
	}, {
		.name = "rndis",
		.usb_config_id = USB_CONFIGURATION_1,
	}, {
		.name = "accessory",
		.usb_config_id = USB_CONFIGURATION_1,
	},
};

static struct slp_multi_platform_data midas_slp_multi_pdata = {
	.nluns	= 2,
	.funcs = midas_slp_multi_funcs,
	.nfuncs = ARRAY_SIZE(midas_slp_multi_funcs),
};

static struct platform_device midas_slp_usb_multi = {
	.name		= "slp_multi",
	.id			= -1,
	.dev		= {
		.platform_data = &midas_slp_multi_pdata,
	},
};
#endif

#ifdef CONFIG_SND_SOC_WM8994
/* vbatt device (for WM8994) */
static struct regulator_consumer_supply vbatt_supplies[] = {
	REGULATOR_SUPPLY("LDO1VDD", NULL),
	REGULATOR_SUPPLY("SPKVDD1", NULL),
	REGULATOR_SUPPLY("SPKVDD2", NULL),
};

static struct regulator_init_data vbatt_initdata = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(vbatt_supplies),
	.consumer_supplies = vbatt_supplies,
};

static struct fixed_voltage_config vbatt_config = {
	.init_data = &vbatt_initdata,
	.microvolts = 5000000,
	.supply_name = "VBATT",
	.gpio = -EINVAL,
};

static struct platform_device vbatt_device = {
	.name = "reg-fixed-voltage",
	.id = -1,
	.dev = {
		.platform_data = &vbatt_config,
	},
};

/* I2C GPIO: GPF0(0/1) for CODEC_SDA/SCL */
static struct regulator_consumer_supply wm1811_ldo1_supplies[] = {
	REGULATOR_SUPPLY("AVDD1", NULL),
};

static struct regulator_init_data wm1811_ldo1_initdata = {
	.constraints = {
		.name = "WM1811 LDO1",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo1_supplies),
	.consumer_supplies = wm1811_ldo1_supplies,
};

static struct regulator_consumer_supply wm1811_ldo2_supplies[] = {
	REGULATOR_SUPPLY("DCVDD", NULL),
};

static struct regulator_init_data wm1811_ldo2_initdata = {
	.constraints = {
		.name = "WM1811 LDO2",
		.always_on = true, /* Actually status changed by LDO1 */
	},
	.num_consumer_supplies = ARRAY_SIZE(wm1811_ldo2_supplies),
	.consumer_supplies = wm1811_ldo2_supplies,
};

static struct wm8994_pdata wm1811_pdata = {
	.gpio_defaults = {
		[0] = WM8994_GP_FN_IRQ,   /* GPIO1 IRQ output, CMOS mode */
		[7] = WM8994_GPN_DIR | WM8994_GP_FN_PIN_SPECIFIC, /* DACDAT3 */
		[8] = WM8994_CONFIGURE_GPIO |
			  WM8994_GP_FN_PIN_SPECIFIC, /* ADCDAT3 */
		[9] = WM8994_CONFIGURE_GPIO |\
			  WM8994_GP_FN_PIN_SPECIFIC, /* LRCLK3 */
		[10] = WM8994_CONFIGURE_GPIO |\
			   WM8994_GP_FN_PIN_SPECIFIC, /* BCLK3 */
	},

	.irq_base = IRQ_BOARD_CODEC_START,

	/* The enable is shared but assign it to LDO1 for software */
	.ldo = {
		{
			.enable = GPIO_WM8994_LDO,
			.init_data = &wm1811_ldo1_initdata,
		},
		{
			.init_data = &wm1811_ldo2_initdata,
		},
	},

	/* Support external capacitors */
	/* This works on wm1811a only (board REV06 or above) */
	.jd_ext_cap = 1,

	/* Regulated mode at highest output voltage */
	/* 2.6V for micbias2 */
	.micbias = {0x2f, 0x2b},

	.micd_lvl_sel = 0xFF,

	.ldo_ena_always_driven = true,
	.ldo_ena_delay = 30000,

	/* Disable ground loop noise feedback on lineout1 - NC - */
	.lineout1fb = 0,
	/* Enable ground loop noise feedback on lineout2 dock audio */
	.lineout2fb = 1,
};
#endif

static struct i2c_gpio_platform_data gpio_i2c_codec = {
	.sda_pin = EXYNOS4_GPF0(0),
	.scl_pin = EXYNOS4_GPF0(1),
};

static struct platform_device device_i2c_codec = {
	.name = "i2c-gpio",
	.id = I2C_CODEC,
	.dev.platform_data = &gpio_i2c_codec,
};

static struct i2c_board_info i2c_devs_codec[] __initdata = {
#ifdef CONFIG_SND_SOC_WM8994
	{
		I2C_BOARD_INFO("wm1811", (0x34 >> 1)),	/* Audio CODEC */
		.platform_data = &wm1811_pdata,
	},
#endif
};

/* I2C4's GPIO: PQ_LTE(CMC_CS) / PQ(NC) / PQ Rev01 (codec) */
static struct i2c_board_info i2c_devs4[] __initdata = {
#if defined(CONFIG_MACH_SLP_PQ) && \
	defined(CONFIG_SND_SOC_WM8994)
	{
		I2C_BOARD_INFO("wm1811", (0x34 >> 1)),	/* Audio CODEC */
		.platform_data = &wm1811_pdata,
		.irq = IRQ_EINT(30),
	},
#endif
};

/* I2C GPIO: NFC */
static struct i2c_gpio_platform_data gpio_i2c_nfc = {
#ifdef CONFIG_MACH_SLP_PQ
	.sda_pin = GPIO_NFC_SDA_18V,
	.scl_pin = GPIO_NFC_SCL_18V,
#elif defined(CONFIG_MACH_SLP_PQ_LTE)
	.sda_pin = EXYNOS4212_GPM4(1),
	.scl_pin = EXYNOS4212_GPM4(0),
#endif
};

static struct platform_device device_i2c_nfc = {
	.name = "i2c-gpio",
	.id = I2C_NFC,
	.dev.platform_data = &gpio_i2c_nfc,
};

/* Bluetooth */
static struct platform_device bcm4334_bluetooth_device = {
	.name = "bcm4334_bluetooth",
	.id = -1,
};

#ifdef CONFIG_MACH_SLP_PQ
/* BCM47511 GPS */
static struct bcm47511_platform_data midas_bcm47511_data = {
	.regpu		= GPIO_GPS_PWR_EN,	/* XM0DATA[15] */
	.nrst		= GPIO_GPS_nRST,	/* XM0DATA[14] */
	.uart_rxd	= GPIO_GPS_RXD,		/* XURXD[1] */
	.gps_cntl	= -1,	/* GPS_CNTL - XM0ADDR[6] */
	.reg32khz	= "lpo_in",
};

static struct platform_device midas_bcm47511 = {
	.name	= "bcm47511",
	.id	= -1,
	.dev	= {
		.platform_data	= &midas_bcm47511_data,
	},
};
#endif

/* I2C GPIO: 3_TOUCH */
static struct i2c_gpio_platform_data gpio_i2c_3_touch = {
	.sda_pin = GPIO_3_TOUCH_SDA,
	.scl_pin = GPIO_3_TOUCH_SCL,
};

static struct platform_device device_i2c_3_touch = {
	.name = "i2c-gpio",
	.id = I2C_3_TOUCH,
	.dev.platform_data = &gpio_i2c_3_touch,
};

static struct i2c_board_info i2c_devs_3_touch[] __initdata = {
	{
		I2C_BOARD_INFO("melfas-touchkey", 0x20),
	},
};

#define GPIO_KEYS(_code, _gpio, _active_low, _iswake, _hook)		\
{					\
	.code = _code,			\
	.gpio = _gpio,	\
	.active_low = _active_low,		\
	.type = EV_KEY,			\
	.wakeup = _iswake,		\
	.debounce_interval = 10,	\
	.isr_hook = _hook,			\
	.value = 1 \
}

static struct gpio_keys_button midas_buttons[] = {
	GPIO_KEYS(KEY_VOLUMEUP, GPIO_VOL_UP,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_VOLUMEDOWN, GPIO_VOL_DOWN,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_POWER, GPIO_nPOWER,
		  1, 1, sec_debug_check_crash_key),
};

static struct gpio_keys_button midas_06_buttons[] = {
	GPIO_KEYS(KEY_VOLUMEUP, GPIO_VOL_UP_00,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_VOLUMEDOWN, GPIO_VOL_DOWN_00,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_POWER, GPIO_nPOWER,
		  1, 1, sec_debug_check_crash_key),
};

static struct gpio_keys_button midas_10_buttons[] = {
	GPIO_KEYS(KEY_VOLUMEUP, GPIO_VOL_UP_00,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_VOLUMEDOWN, GPIO_VOL_DOWN_00,
		  1, 0, sec_debug_check_crash_key),
	GPIO_KEYS(KEY_POWER, GPIO_nPOWER,
		  1, 1, sec_debug_check_crash_key),
/*
 * keep this code for future use.
 */
	GPIO_KEYS(KEY_MENU, GPIO_OK_KEY_ANDROID,
		  1, 1, sec_debug_check_crash_key),
};

static struct gpio_keys_platform_data midas_gpiokeys_platform_data = {
	.buttons = midas_buttons,
	.nbuttons = ARRAY_SIZE(midas_buttons),
};

static struct platform_device midas_keypad = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data = &midas_gpiokeys_platform_data,
	},
};

#ifdef CONFIG_I2C_SI4705
static void pq_si4705_reset(int enable)
{
	pr_info("%s: enable is %d", __func__, enable);
	if (enable)
		gpio_set_value(GPIO_FM_RST, 1);
	else
		gpio_set_value(GPIO_FM_RST, 0);
}

static void pq_si4705_init(void)
{
	gpio_request(GPIO_FM_RST, "fmradio_reset");
	s3c_gpio_cfgpin(GPIO_FM_RST, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_FM_RST, 0);
}

static struct i2c_gpio_platform_data gpio_i2c_fm_radio = {
	.sda_pin	= EXYNOS4_GPY0(3),
	.scl_pin	= EXYNOS4_GPY0(2),
};

static struct platform_device device_i2c_fm_radio = {
	.name = "i2c-gpio",
	.id = I2C_FM_RADIO,
	.dev.platform_data = &gpio_i2c_fm_radio,
};

static struct si4705_pdata pq_fm_radio_info = {
	.reset = pq_si4705_reset,
	.pdata_values = (SI4705_PDATA_BIT_VOL_STEPS |
			 SI4705_PDATA_BIT_VOL_TABLE |
			 SI4705_PDATA_BIT_RSSI_THRESHOLD |
			 SI4705_PDATA_BIT_SNR_THRESHOLD),
	.rx_vol_steps = 16,
	.rx_vol_table = {	0x0, 0x13, 0x16, 0x19,
				0x1C, 0x1F, 0x22, 0x25,
				0x28, 0x2B, 0x2E, 0x31,
				0x34, 0x37, 0x3A, 0x3D	},
	.rx_seek_tune_rssi_threshold = 0x00,
	.rx_seek_tune_snr_threshold = 0x01,
};

static struct i2c_board_info i2c_devs_fm_radio[] __initdata = {
	{
		I2C_BOARD_INFO("si4705", 0x22>>1),
		.platform_data = &pq_fm_radio_info,
		.irq = IRQ_EINT(11),
	}
};
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
static struct samsung_battery_platform_data samsung_battery_pdata = {
	.charger_name	= "max77693-charger",
	.fuelgauge_name	= "max17047-fuelgauge",
	.voltage_max = 4200000,
	.voltage_min = 3400000,

	.in_curr_limit = 1000,
	.chg_curr_ta = 1000,

	.chg_curr_usb = 475,
	.chg_curr_cdp = 1000,
	.chg_curr_wpc = 475,
	.chg_curr_dock = 1000,
	.chg_curr_etc = 475,
#ifdef CONFIG_BATTERY_SIOP_LEVEL
	.chg_curr_siop_lv1 = 475,
	.chg_curr_siop_lv2 = 475,
	.chg_curr_siop_lv3 = 475,
#endif
	.chng_interval = 30,
	.chng_susp_interval = 60,
	.norm_interval = 120,
	.norm_susp_interval = 7200,
	.emer_lv1_interval = 30,
	.emer_lv2_interval = 10,

	.recharge_voltage = 4150000,	/* it will be cacaluated in probe */

	.abstimer_charge_duration = 6 * 60 * 60,
	.abstimer_charge_duration_wpc = 8 * 60 * 60,
	.abstimer_recharge_duration = 1.5 * 60 * 60,

	.cb_det_src = CABLE_DET_CHARGER,
	.overheat_stop_temp = 600,
	.overheat_recovery_temp = 400,
	.freeze_stop_temp = -50,
	.freeze_recovery_temp = 0,

	/* CTIA spec */
	.ctia_spec  = false,

	/* CTIA temperature spec */
	.event_time = 10 * 60,
	.event_overheat_stop_temp = 600,
	.event_overheat_recovery_temp = 400,
	.event_freeze_stop_temp = -50,
	.event_freeze_recovery_temp = 0,
	.lpm_overheat_stop_temp = 480,
	.lpm_overheat_recovery_temp = 450,
	.lpm_freeze_stop_temp = -50,
	.lpm_freeze_recovery_temp = 0,

	.temper_src = TEMPER_AP_ADC,
	.temper_ch = 2,
#ifdef CONFIG_S3C_ADC
	/* s3c adc driver does not convert raw adc data.
	 * so, register convert function.
	 */
	.covert_adc = convert_adc,
#endif

	.vf_det_src = VF_DET_CHARGER,
	.vf_det_ch = 0,	/* if src == VF_DET_ADC */
	.vf_det_th_l = 500,
	.vf_det_th_h = 1500,

	.suspend_chging = true,

	.led_indicator = false,

	.battery_standever = false,
};

static struct platform_device samsung_device_battery = {
	.name	= "samsung-battery",
	.id	= -1,
	.dev.platform_data = &samsung_battery_pdata,
};
#endif


/* I2C GPIO: Fuel Gauge */
static struct i2c_gpio_platform_data gpio_i2c_fuel = {
	/* PQ/PQLTE use GPF1(4, 5) */
	.sda_pin = GPIO_FUEL_SDA,
	.scl_pin = GPIO_FUEL_SCL,
};

static struct platform_device device_i2c_fuel = {
	.name = "i2c-gpio",
	.id = I2C_FUEL,
	.dev.platform_data = &gpio_i2c_fuel,
};

static struct max17047_platform_data max17047_pdata = {
	.irq_gpio = GPIO_FUEL_ALERT,
};

static struct i2c_board_info i2c_devs_fuel[] __initdata = {
	{
		I2C_BOARD_INFO("max17047-fuelgauge", 0x36),
		.platform_data = &max17047_pdata,
	},
};

/* I2C GPIO: Barometer (BSENSE) */
static struct i2c_gpio_platform_data gpio_i2c_bsense = {
	.sda_pin = GPIO_BSENSE_SDA_18V,
	.scl_pin = GPIO_BENSE_SCL_18V,
};

static struct platform_device device_i2c_bsense = {
	.name = "i2c-gpio",
	.id = I2C_BSENSE,
	.dev.platform_data = &gpio_i2c_bsense,
};

static struct lps331ap_platform_data lps331_pdata = {
	.irq = GPIO_BARO_INT,
};

static struct i2c_board_info i2c_devs_bsense[] __initdata = {
	{
		I2C_BOARD_INFO(LPS331AP_PRS_DEV_NAME, LPS331AP_PRS_I2C_SAD_H),
		.platform_data = &lps331_pdata,
	},
};

static void lps331ap_gpio_init(void)
{
	int ret = gpio_request(GPIO_BARO_INT, "lps331_irq");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio lps331_irq\n");

	s3c_gpio_cfgpin(GPIO_BARO_INT, S3C_GPIO_INPUT);
	gpio_set_value(GPIO_BARO_INT, 2);
	s3c_gpio_setpull(GPIO_BARO_INT, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(GPIO_BARO_INT, S5P_GPIO_DRVSTR_LV1);
};

/* Magnetic (MSENSE) Sensor */
static int magnetic_get_position(void)
{
	int position;

	if (board_is_redwood()) {
		if (hwrevision(REDWOOD_REV0_1_0425))
			position = 2;
		else if (hwrevision(REDWOOD_REAL_REV0_0))
			position = 7;
		else
			position = 6;
	} else {
		if  (hwrevision(M0_REAL_REV1_1))
			position = 0; /* 0 top/upper-left */
		else
			position = 2; /* 2 top/lower-right */
	}

	return position;
}

/* I2C GPIO: Magnetic (MSENSE) */
static struct i2c_gpio_platform_data gpio_i2c_msense = {
	.sda_pin = GPIO_MSENSOR_SDA_18V,
	.scl_pin = GPIO_MSENSOR_SCL_18V,
	.udelay = 2, /* 250KHz */
};

static struct platform_device device_i2c_msense = {
	.name = "i2c-gpio",
	.id = I2C_MSENSE,
	.dev.platform_data = &gpio_i2c_msense,
};

static void akm8963_reset(void)
{
	gpio_set_value(GPIO_MSENSE_RST_N, 0);
	udelay(5);
	gpio_set_value(GPIO_MSENSE_RST_N, 1);
	/* Device will be accessible 100 us after */
	udelay(100);
}

static struct akm8963_platform_data akm8963_pdata = {
	.magnetic_get_position = magnetic_get_position,
	.akm8963_reset = akm8963_reset,
	.gpio_data_ready_int = GPIO_MSENSOR_INT,
	.layout = 1,
	.outbit = 1,
	.gpio_rst = GPIO_MSENSE_RST_N,
};

static struct akm8975_platform_data akm8975_pdata = {
	.magnetic_get_position = magnetic_get_position,
#ifdef CONFIG_MACH_SLP_PQ
	.gpio_data_ready_int = GPIO_MSENSOR_INT,
#else
	/* CONFIG_MACH_SLP_PQ_LTE */
	.gpio_data_ready_int = EXYNOS4_GPX2(2),
#endif
};

#ifdef CONFIG_BUSFREQ_OPP
static struct device_domain busfreq;

static struct platform_device exynos4_busfreq = {
	.id = -1,
	.name = "exynos-busfreq",
};
#endif

static struct i2c_board_info i2c_devs_msense_ak8963[] __initdata = {
	{
		I2C_BOARD_INFO("ak8963", 0x0C),
		.platform_data = &akm8963_pdata,
	},
};

static struct i2c_board_info i2c_devs_msense_ak8975[] __initdata = {
	{
		I2C_BOARD_INFO("ak8975", 0x0C),
		.platform_data = &akm8975_pdata,
	},
};

#ifdef CONFIG_MACH_SLP_PQ
/* magnetic senso  gpio init */
static void __init msense_gpio_init(void)
{
	int ret = gpio_request(GPIO_MSENSOR_INT, "gpio_akm_int");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio akm_int.\n");

	if (board_is_redwood()) {
		s3c_gpio_cfgpin(GPIO_MSENSE_RST_N, S3C_GPIO_OUTPUT);
		gpio_set_value(GPIO_MSENSE_RST_N, 1);
		s3c_gpio_setpull(GPIO_MSENSE_RST_N, S3C_GPIO_PULL_NONE);
	}

	s5p_register_gpio_interrupt(GPIO_MSENSOR_INT);
	s3c_gpio_setpull(GPIO_MSENSOR_INT, S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(GPIO_MSENSOR_INT, S3C_GPIO_SFN(0xF));

	if (board_is_redwood())
		i2c_devs_msense_ak8963[0].irq = gpio_to_irq(GPIO_MSENSOR_INT);
	else
		i2c_devs_msense_ak8975[0].irq = gpio_to_irq(GPIO_MSENSOR_INT);
}
#endif

/* I2C GPIO: MHL */
static struct i2c_gpio_platform_data gpio_i2c_mhl = {
	.sda_pin = GPIO_MHL_SDA_1_8V,
	.scl_pin = GPIO_MHL_SCL_1_8V,
	.udelay = 3,
};

static struct platform_device device_i2c_mhl = {
	.name = "i2c-gpio",
	.id = I2C_MHL,
	.dev.platform_data = &gpio_i2c_mhl,
};

/* I2C GPIO: MHL_D */
static struct i2c_gpio_platform_data gpio_i2c_mhl_d = {
	.sda_pin = GPIO_MHL_DSDA_2_8V,
	.scl_pin = GPIO_MHL_DSCL_2_8V,
};

static struct platform_device device_i2c_mhl_d = {
	.name = "i2c-gpio",
	.id = I2C_MHL_D,
	.dev.platform_data = &gpio_i2c_mhl_d,
};

/* I2C GPIO: PS_ALS (PSENSE) */
static struct i2c_gpio_platform_data gpio_i2c_psense_cm36651 = {
	.sda_pin = GPIO_RGB_SDA_1_8V,
	.scl_pin = GPIO_RGB_SCL_1_8V,
	.udelay = 2, /* 250KHz */
};

static struct platform_device device_i2c_psense_cm36651 = {
	.name = "i2c-gpio",
	.id = I2C_PSENSE,
	.dev.platform_data = &gpio_i2c_psense_cm36651,
};

/* I2C GPIO: PS_ALS (PSENSE) */
static struct i2c_gpio_platform_data gpio_i2c_psense_gp2a = {
	.sda_pin = GPIO_PS_ALS_SDA_28V,
	.scl_pin = GPIO_PS_ALS_SCL_28V,
	.udelay = 2, /* 250KHz */
};

static struct platform_device device_i2c_psense_gp2a = {
	.name = "i2c-gpio",
	.id = I2C_PSENSE,
	.dev.platform_data = &gpio_i2c_psense_gp2a,
};

static int proximity_leda_on(bool onoff)
{
	printk(KERN_INFO "%s, onoff = %d\n", __func__, onoff);

	gpio_set_value(GPIO_PS_ALS_EN, onoff);

	return 0;
}

static int gp2a_check_version(void)
{
	if (system_rev != 0x00 && system_rev != 0x03)
		return 1;

	return 0;
}

static struct cm36651_platform_data cm36651_pdata = {
	.cm36651_led_on = proximity_leda_on,
	.irq = GPIO_PS_ALS_INT,
};

static struct i2c_board_info i2c_devs_psense_gp2a[] __initdata = {
	{
		I2C_BOARD_INFO("gp2a", (0x72 >> 1)),
	},
};

static struct i2c_board_info i2c_devs_psense_cm36651[] __initdata = {
	{
		I2C_BOARD_INFO("cm36651", (0x30 >> 1)),
		.platform_data = &cm36651_pdata,
	},
};

static struct gp2a_platform_data gp2a_pdata = {
	.gp2a_led_on	= proximity_leda_on,
	.p_out = GPIO_PS_ALS_INT,
	.gp2a_check_version = gp2a_check_version,
};

static struct platform_device opt_gp2a = {
	.name = "gp2a-opt",
	.id = -1,
	.dev = {
		.platform_data = &gp2a_pdata,
	},
};

static void optical_gpio_init(void)
{
	int ret = gpio_request(GPIO_PS_ALS_EN, "optical_power_supply_on");

	printk(KERN_INFO "%s\n", __func__);

	if (ret)
		printk(KERN_ERR "Failed to request gpio optical power supply.\n");

	/* configuring for gp2a gpio for LEDA power */
	s3c_gpio_cfgpin(GPIO_PS_ALS_EN, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_PS_ALS_EN, 0);
	s3c_gpio_setpull(GPIO_PS_ALS_EN, S3C_GPIO_PULL_NONE);

	s3c_gpio_setpull(GPIO_PS_ALS_INT, S3C_GPIO_PULL_UP);
}

static struct jack_platform_data midas_jack_data = {
	.usb_online		= 0,
	.charger_online	= 0,
	.hdmi_online	= 0,
	.earjack_online	= 0,
	.earkey_online	= 0,
	.ums_online		= -1,
	.cdrom_online	= -1,
	.jig_online		= -1,
	.host_online	= 0,
};

static struct platform_device midas_jack = {
	.name		= "jack",
	.id			= -1,
	.dev		= {
		.platform_data = &midas_jack_data,
	},
};

#if defined(CONFIG_ARM_EXYNOS4_BUS_DEVFREQ)
static struct exynos4_bus_platdata devfreq_bus_pdata = {
	.threshold = {
		.upthreshold = 90,
		.downdifferential = 10,
	},
	.polling_ms = 50,
};
static struct platform_device devfreq_busfreq = {
	.name		= "exynos4412-busfreq",
	.id		= -1,
	.dev		= {
		.platform_data = &devfreq_bus_pdata,
	},
};
#endif

/* Uart Select */
static void midas_set_uart_switch(int path)
{
	int gpio;

	gpio = EXYNOS4_GPF2(3);
	gpio_request(gpio, "UART_SEL");

	/* gpio_high == AP */
	if (path == UART_SW_PATH_AP)
		gpio_set_value(gpio, GPIO_LEVEL_HIGH);
	else if (path == UART_SW_PATH_CP)
		gpio_set_value(gpio, GPIO_LEVEL_LOW);

	gpio_free(gpio);
	return;
}

static int midas_get_uart_switch(void)
{
	int val;
	int gpio;

	gpio = EXYNOS4_GPF2(3);
	gpio_request(gpio, "UART_SEL");
	val = gpio_get_value(gpio);
	gpio_free(gpio);

	/* gpio_high == AP */
	if (val == GPIO_LEVEL_HIGH)
		return UART_SW_PATH_AP;
	else if (val == GPIO_LEVEL_LOW)
		return UART_SW_PATH_CP;
	else
		return UART_SW_PATH_NA;
}

static struct uart_select_platform_data midas_uart_select_data = {
	.set_uart_switch	= midas_set_uart_switch,
	.get_uart_switch	= midas_get_uart_switch,
};

static struct platform_device midas_uart_select = {
	.name			= "uart-select",
	.id			= -1,
	.dev			= {
		.platform_data	= &midas_uart_select_data,
	},
};

#ifdef CONFIG_INPUT_SECBRIDGE
/*
 * sec-input-bridge
 */
static const struct sec_input_bridge_mkey pq_appslog_mkey_map[] = {
	{ .type = EV_KEY , .code = KEY_VOLUMEUP			},
	{ .type = EV_KEY , .code = KEY_VOLUMEDOWN			},
	{ .type = EV_KEY , .code = KEY_VOLUMEUP			},
	{ .type = EV_KEY , .code = KEY_VOLUMEDOWN			},
	{ .type = EV_KEY , .code = KEY_POWER				},
	{ .type = EV_KEY , .code = KEY_VOLUMEDOWN			},
	{ .type = EV_KEY , .code = KEY_VOLUMEUP			},
	{ .type = EV_KEY , .code = KEY_POWER				},
};

static const struct sec_input_bridge_mmap pq_mmap[] = {
	{
		.mkey_map = pq_appslog_mkey_map,
		.num_mkey = ARRAY_SIZE(pq_appslog_mkey_map),
		.uevent_env_str = "APPS_LOG",
		.enable_uevent = 1,
		.uevent_action = KOBJ_CHANGE,
		.uevent_env_value = "ON",
		},
};

static struct sec_input_bridge_platform_data pq_input_bridge_data = {
	.mmap = pq_mmap,
	.num_map = ARRAY_SIZE(pq_mmap),
	/* .lcd_warning_func = lcd_warning_function,*/
};

static struct platform_device pq_input_bridge = {
	.name	= "samsung_input_bridge",
	.id	= -1,
	.dev	= {
		.platform_data = &pq_input_bridge_data,
			},
};
#endif


static struct platform_device *slp_midas_devices[] __initdata = {
	/* Samsung Power Domain */
	&exynos4_device_pd[PD_MFC],
	&exynos4_device_pd[PD_G3D],
	&exynos4_device_pd[PD_LCD0],
	&exynos4_device_pd[PD_CAM],
	&exynos4_device_pd[PD_TV],
	&exynos4_device_pd[PD_GPS],
	&exynos4_device_pd[PD_GPS_ALIVE],
	&exynos4_device_pd[PD_ISP],

	&s3c_device_wdt,
	&s3c_device_rtc,
	&s3c_device_i2c0,	/* PQ_LTE only: 8M CAM */
	&s3c_device_i2c1,	/* Gyro/Acc */
	/* i2c2: used by GPS UART */
	&s3c_device_i2c3,	/* Meltas TSP */
	/* i2c4: NC(PQ) / codec: wm1811 (PQ rev01) / Modem(PQ LTE) */
	&s3c_device_i2c4,
	/* i2c5: NC(PQ) / Modem(PQ LTE) */
	&s3c_device_i2c7,	/* MAX77686 PMIC */
	&device_i2c_3_touch,	/* PQ_LTE only: Meltas Touchkey */
#ifdef CONFIG_I2C_SI4705
	&device_i2c_fm_radio,
#endif
	&device_i2c_if_pmic,	/* if_pmic: max77693 */
	&device_i2c_fuel,	/* max17047-fuelgauge */
	&device_i2c_bsense,	/* barometer lps331ap */
	&device_i2c_msense, /* magnetic ak8975c */
	&device_i2c_mhl,
	/* TODO: SW I2C for 8M CAM of PQ (same gpio with PQ_LTE NFC) */
	/* TODO: SW I2C for VT_CAM (GPIO_VT_CAM_SCL/SDA) */
	/* TODO: SW I2C for ADC (GPIO_ADC_SCL/SDA) */
	/* TODO: SW I2C for LTE of PQ_LTE (F2(4) SDA, F2(5) SCL) */

#ifdef CONFIG_DRM_EXYNOS_FIMD
	&s5p_device_fimd0,
#endif
#ifdef CONFIG_DRM_EXYNOS_HDMI
	&s5p_device_i2c_hdmiphy,
	&s5p_device_hdmi,
	&s5p_device_mixer,
	&exynos_drm_hdmi_device,
#endif
	&exynos_drm_vidi_device,
#ifdef CONFIG_DRM_EXYNOS_IPP
	&exynos_drm_ipp_device,
#endif
#ifdef CONFIG_DRM_EXYNOS_G2D
	&s5p_device_fimg2d,
#endif
#ifdef CONFIG_DRM_EXYNOS_ROTATOR
	&exynos_device_rotator,
#endif
#ifdef CONFIG_DRM_EXYNOS_FIMC
	&exynos_drm_fimc2_device,
	&exynos_drm_fimc3_device,
#endif
#ifdef CONFIG_DRM_EXYNOS
	&exynos_drm_device,
#endif
#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif

#ifdef CONFIG_SND_SOC_WM8994
	&vbatt_device,
#endif
	&samsung_asoc_dma,
#ifndef CONFIG_SND_SOC_SAMSUNG_USE_DMA_WRAPPER
	&samsung_asoc_idma,
#endif

#ifdef CONFIG_SND_SAMSUNG_AC97
	&exynos_device_ac97,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	&exynos_device_pcm0,
#endif
#ifdef CONFIG_SND_SAMSUNG_SPDIF
	&exynos_device_spdif,
#endif
#if defined(CONFIG_SND_SAMSUNG_RP) || defined(CONFIG_SND_SAMSUNG_ALP)
	&exynos_device_srp,
#endif
#if defined CONFIG_USB_EHCI_S5P && !defined CONFIG_LINK_DEVICE_HSIC
	&s5p_device_ehci,
#endif
#if defined CONFIG_USB_OHCI_S5P && !defined CONFIG_LINK_DEVICE_HSIC
	&s5p_device_ohci,
#endif
#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_G_SLP
	&midas_slp_usb_multi,
#endif
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	&exynos_device_dwmci,
#else
	&s3c_device_mshci,
#endif
	&s3c_device_hsmmc2,
	&s3c_device_hsmmc3,

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&exynos4_device_fimc_is,
#endif
#ifdef CONFIG_VIDEO_FIMC
	&s3c_device_fimc0,
	&s3c_device_fimc1,
#ifndef CONFIG_DRM_EXYNOS_FIMC
	&s3c_device_fimc2,
	&s3c_device_fimc3,
#endif
#elif defined(CONFIG_VIDEO_SAMSUNG_S5P_FIMC)
	&s5p_device_fimc0,
	&s5p_device_fimc1,
#ifndef CONFIG_DRM_EXYNOS_FIMC
	&s5p_device_fimc2,
	&s5p_device_fimc3,
#endif
#endif
#if defined(CONFIG_VIDEO_FIMC_MIPI)
	&s3c_device_csis0,
	&s3c_device_csis1,
#endif
#if defined(CONFIG_VIDEO_MFC5X) || defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	&s5p_device_mfc,
#endif
#ifdef CONFIG_S5P_SYSTEM_MMU
	&SYSMMU_PLATDEV(fimd0),
	&SYSMMU_PLATDEV(tv),
	&SYSMMU_PLATDEV(g2d_acp),
#ifdef CONFIG_DRM_EXYNOS_ROTATOR
	&SYSMMU_PLATDEV(rot),
#endif
#ifdef CONFIG_DRM_EXYNOS_FIMC
	&SYSMMU_PLATDEV(fimc2),
	&SYSMMU_PLATDEV(fimc3),
#endif
	&SYSMMU_PLATDEV(mfc_l),
	&SYSMMU_PLATDEV(mfc_r),
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	&exynos_device_flite0,
	&exynos_device_flite1,
#endif
#ifdef CONFIG_CHARGER_MANAGER
	&midas_charger_manager,
#endif
#if defined(CONFIG_BATTERY_SAMSUNG)
	&samsung_device_battery,
#endif
#ifdef CONFIG_SENSORS_NTC_THERMISTOR
	&midas_ncp15wb473_thermistor,
#endif
#ifdef CONFIG_VIDEO_JPEG_V2X
	&s5p_device_jpeg,
#endif
	&midas_keypad,
	&midas_jack,
	&midas_uart_select,
	&bcm4334_bluetooth_device,
#if defined(CONFIG_S3C64XX_DEV_SPI)
	&exynos_device_spi1,
#endif
#ifdef CONFIG_MACH_SLP_PQ
	&midas_bcm47511,
#endif
#if defined(CONFIG_ARM_EXYNOS4_BUS_DEVFREQ)
	&devfreq_busfreq,
#endif

#if defined(CONFIG_BUSFREQ_OPP)
	&exynos4_busfreq,
#endif

#ifdef CONFIG_EXYNOS4_SETUP_THERMAL
	&s5p_device_tmu,
#else
	&exynos4_device_tmu,
#endif
	&host_notifier_device,
#ifdef CONFIG_INPUT_SECBRIDGE
	&pq_input_bridge,
#endif

};

static void check_hw_revision(void)
{
	unsigned int hwrev = system_rev & 0xff;

	switch (hwrev) {
	case M0_PROXIMA_REV0_0:	/* Proxima Rev0.0: M0_PROXIMA_REV0.0_1114 */
	case M0_PROXIMA_REV0_1:	/* Proxima Rev0.1: M0_PROXIMA_REV0.1_1125 */
	#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS
		melfas_tsp_set_platdata(TSP_POSITION_DEFAULT);
		melfas_tsp_init();
	#endif
		/* VOL_UP/DOWN keys are not EXTINT. Register them. */
		s5p_register_gpio_interrupt(GPIO_VOL_UP);
		s5p_register_gpio_interrupt(GPIO_VOL_DOWN);
		break;
	case M0_REAL_REV0_6:	/* Proxima Rev0.6: M0_REAL_REV0.6_120119 */
	case M0_REAL_REV0_6_A:	/* Proxima Rev0.6: M0_REAL_REV0.6_A */
	#ifdef CONFIG_LEDS_AAT1290A
		platform_device_register(&s3c_device_aat1290a_led);
	#endif
	#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS
		melfas_tsp_set_platdata(TSP_POSITION_DEFAULT);
		melfas_tsp_init();
	#endif
		midas_gpiokeys_platform_data.buttons = midas_06_buttons;
		midas_gpiokeys_platform_data.nbuttons =
			ARRAY_SIZE(midas_06_buttons);
		/* VOL_UP/DOWN keys are not EXTINT. Register them. */
		s5p_register_gpio_interrupt(GPIO_VOL_UP_00);
		s5p_register_gpio_interrupt(GPIO_VOL_DOWN_00);
		break;
	case SLP_PQ_CMC221_LTE:	/* PegasusQ LTE: SLP_PQ_CMC221_VIA_1028 */
	#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS
		melfas_tsp_set_platdata(TSP_POSITION_ROTATED);
		melfas_tsp_init();
	#endif
		/* VOL_UP/DOWN keys are not EXTINT. Register them. */
		s5p_register_gpio_interrupt(GPIO_VOL_UP);
		s5p_register_gpio_interrupt(GPIO_VOL_DOWN);
		break;
	case M0_REAL_REV1_0:	/* Proxima Rev1.0: M0_REAL_REV1.0_120302 */
	case M0_REAL_REV1_1:	/* M0_REAL_REV1.1: M0_REAL_REV1.1_2nd_120413 */
	case REDWOOD_REAL_REV0_1:
	#ifdef CONFIG_LEDS_AAT1290A
		platform_device_register(&s3c_device_aat1290a_led);
	#endif
	#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS
		melfas_tsp_set_platdata(TSP_POSITION_DEFAULT);
		melfas_tsp_init();
	#endif
		midas_gpiokeys_platform_data.buttons = midas_10_buttons;
		midas_gpiokeys_platform_data.nbuttons =
						ARRAY_SIZE(midas_10_buttons);
		/* VOL_UP/DOWN keys are not EXTINT. Register them. */
		s5p_register_gpio_interrupt(GPIO_VOL_UP_00);
		s5p_register_gpio_interrupt(GPIO_VOL_DOWN_00);

		s5p_register_gpio_interrupt(GPIO_OK_KEY_ANDROID);
	default:
		break;
	}
}

static void i2c_if_pmic_register(void)
{

	unsigned int hwrev = system_rev & 0xff;

	switch (hwrev) {
	case M0_PROXIMA_REV0_0:	/* Proxima Rev0.0: M0_PROXIMA_REV0.0_1114 */
	case M0_PROXIMA_REV0_1:	/* Proxima Rev0.1: M0_PROXIMA_REV0.1_1125 */
	case SLP_PQ_CMC221_LTE:	/* PegasusQ LTE: SLP_PQ_CMC221_VIA_1028 */
		i2c_register_board_info(I2C_IF_PMIC, i2c_devs_if_pmic,
			ARRAY_SIZE(i2c_devs_if_pmic));
		break;

	case M0_REAL_REV0_6:	/* Proxima Rev0.6: M0_REAL_REV0.6_120119 */
	case M0_REAL_REV0_6_A:	/* Proxima Rev0.6: M0_REAL_REV0.6_A */
	case M0_REAL_REV1_0:	/* Proxima Rev1.0: M0_REAL_REV1.0_120302 */
	case M0_REAL_REV1_1:	/* M0_REAL_REV1.1: M0_REAL_REV1.1_2nd_120413 */
	default:
		i2c_register_board_info(I2C_IF_PMIC, i2c_devs_if_pmic_no_led,
			ARRAY_SIZE(i2c_devs_if_pmic_no_led));
		break;
	}
}

#ifdef CONFIG_EXYNOS4_SETUP_THERMAL
/* below temperature base on the celcius degree */
static struct s5p_platform_tmu midas_tmu_data __initdata = {
	.ts = {
		.stop_1st_throttle  = 78,
		.start_1st_throttle = 80,
		.stop_2nd_throttle  = 87,
		.start_2nd_throttle = 103,
		/* temp to do tripping */
		.start_tripping     = 110,
		/* To protect chip,forcely kernel panic */
		.start_emergency    = 120,
		.stop_mem_throttle  = 80,
		.start_mem_throttle = 85,
	},
	.cpufreq = {
		.limit_1st_throttle  = 800000, /* 800MHz in KHz order */
		.limit_2nd_throttle  = 200000, /* 200MHz in KHz order */
	},
	.temp_compensate = {
		/* vdd_arm in uV for temperature compensation */
		.arm_volt = 900000,
		/* vdd_bus in uV for temperature compensation */
		.bus_volt = 900000,
		/* vdd_g3d in uV for temperature compensation */
		.g3d_volt = 900000,
	},
};
#endif

#ifdef CONFIG_LINK_DEVICE_HSIC
static int __init s5p_hci_device_initcall(void)
{
	/*
	 * ehcd should be probed first.
	 * Unless device detected as fullspeed always.
	 */
#ifdef CONFIG_USB_EHCI_S5P
	int ret = platform_device_register(&s5p_device_ehci);
	if (ret)
		return ret;

	/*
	 * Exynos AP-EVT0 can't use both USB host and device(client)
	 * on running time, because that has critical ASIC problem
	 * about USB PHY CLOCK. That issue was already announced by
	 * S.SLI team (djkim@samsung.com) and already fixed it on
	 * the new EVT1 chip (new target, system_rev != 3).
	 * But we have many EVT0 targets (system_rev == 3)
	 * So, to using old target(EVT0) only using by usb device mode
	 * we added following unregister codes(disable USB Host)
	 * by yongsul96.oh@samsung.com 20120417-SLP
	 */
	if (system_rev == 3) {
		pr_warn("[USB-EHCI]AP is EVT0 type!!, unregister ehci!!!");
		platform_device_unregister(&s5p_device_ehci);
	}
#endif
#ifdef CONFIG_USB_OHCI_S5P
	return platform_device_register(&s5p_device_ohci);
#endif
}
late_initcall(s5p_hci_device_initcall);
#endif	/* LINK_DEVICE_HSIC */

#if defined(CONFIG_S5P_MEM_CMA)
static struct cma_region regions[] = {
	/*
	 * caution : do not allowed other region definitions above of drm.
	 * drm only using region 0 for startup screen display.
	 */
#ifdef CONFIG_DRM_EXYNOS
	{
		.name = "drm",
		.size = CONFIG_DRM_EXYNOS_MEMSIZE * SZ_1K,
		.start = 0
	},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_DMA
	{
		.name = "dma",
		.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_DMA * SZ_1K,
		.start = 0
	},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC1
	{
		.name = "mfc1",
		.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC1 * SZ_1K,
		{
			.alignment = 1 << 17,
		},
		.start = 0,
	},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_NORMAL
		{
			.name = "mfc-normal",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_NORMAL * SZ_1K,
			.start = 0,
		},
#endif
#if !defined(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION) && \
	defined(CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC0)
	{
		.name = "mfc0",
		.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC0 * SZ_1K,
		{
			.alignment = 1 << 17,
		},
		.start = 0,
	},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC
	{
		.name = "mfc",
		.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC * SZ_1K,
		{
			.alignment = 1 << 17,
		},
		.start = 0
	},
#endif
#if !defined(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION) && \
	defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	{
		.name		= "b2",
		.size		= 32 << 20,
		{ .alignment	= 128 << 10 },
	},
	{
		.name		= "b1",
		.size		= 32 << 20,
		{ .alignment	= 128 << 10 },
	},
	{
		.name		= "fw",
		.size		= 1 << 20,
		{ .alignment	= 128 << 10 },
		.start		= 0x60500000,	/* FIXME */
	},
#endif
#ifdef CONFIG_AUDIO_SAMSUNG_MEMSIZE_SRP
	{
		.name = "srp",
		.size = CONFIG_AUDIO_SAMSUNG_MEMSIZE_SRP * SZ_1K,
		.start = 0,
	},
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	{
		.name = "fimc_is",
		.size = CONFIG_VIDEO_EXYNOS_MEMSIZE_FIMC_IS * SZ_1K,
		{
			.alignment = 1 << 26,
		},
		.start = 0
	},
#endif
	{
		.size = 0
	},
};

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	static struct cma_region regions_secure[] = {
		{
			.name	= "fimc-secure",
			.size	= 14 * SZ_1M,
		},
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_SECURE
		{
			.name = "mfc-secure",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_SECURE * SZ_1K,
		},
#endif
		{
			.name = "sectbl",
			.size = SZ_1M,
		},
		{
			.size = 0
		},
	};
#else /* !CONFIG_EXYNOS_CONTENT_PATH_PROTECTION */
	struct cma_region *regions_secure = NULL;
#endif

static void __init exynos4_reserve_mem(void)
{
	static const char map[] __initconst =
#ifdef CONFIG_DRM_EXYNOS
		"exynos-drm=drm;"
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_DMA
		"s3c-fimc.0=dma;s3c-fimc.1=dma;s3c-fimc.2=dma;s3c-fimc.3=dma;s3c-mem=dma;"
		"exynos4210-fimc.0=dma;exynos4210-fimc.1=dma;exynos4210-fimc.2=dma;exynos4210-fimc.3=dma;"
#endif
#ifdef CONFIG_VIDEO_MFC5X
		"s3c-mfc/A=mfc0,mfc-secure;"
		"s3c-mfc/B=mfc1,mfc-normal;"
		"s3c-mfc/AB=mfc;"
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_S5P_MFC
		"s5p-mfc/f=fw;"
		"s5p-mfc/a=b1;"
		"s5p-mfc/b=b2;"
#endif
		"samsung-rp=srp;"
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
		"exynos4-fimc-is=fimc_is;"
#endif
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		"s5p-smem/sectbl=sectbl;"
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_SECURE
		"s5p-smem/mfc=mfc-secure;"
#endif
		"s5p-smem/fimc=fimc-secure;"
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_NORMAL
		"s5p-smem/mfc-shm=mfc-normal;"
#endif
		"s5p-smem/fimd=drm;"
		""
	;

	if (fbmem_start) {
		int i = 0, drm = 0, dma = 0, mfc1 = 0, mfc0 = 0;
		regions[drm].start = (dma_addr_t) fbmem_start;
		for (i = 0; i < ARRAY_SIZE(regions) - 1 /* terminator */; ++i) {
			if (strncmp(regions[i].name, "dma",
				strlen(regions[i].name)) == 0) {
				dma = i;
				regions[dma].start =
					(dma_addr_t) regions[drm].start
					-regions[dma].size;
			}
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
			if (strncmp(regions[i].name, "mfc-normal",
				strlen(regions[i].name)) == 0) {
				mfc1 = i;
				regions[mfc1].start =
					(dma_addr_t) regions[dma].start
					-regions[mfc1].size;
			}
#else
			if (strncmp(regions[i].name, "mfc1",
				strlen(regions[i].name)) == 0) {
				mfc1 = i;
				regions[mfc1].start =
					(dma_addr_t) regions[dma].start
					-regions[mfc1].size;
			}
#endif
			if (strncmp(regions[i].name, "mfc0",
				strlen(regions[i].name)) == 0) {
				mfc0 = i;
				regions[mfc0].start =
					(dma_addr_t) regions[mfc1].start
					-regions[mfc0].size;
			}

		}
	}

	s5p_cma_region_reserve(regions, regions_secure, 0, map);
}

#ifdef CONFIG_HIBERNATION
static int __init exynos_set_nosave_regions(void)
{
	int i;

	for (i = ARRAY_SIZE(regions) - 2; i >= 0 /* terminator */; i--) {
		/*
		 * MFC firmware region SHOULD BE saved.
		 * If the name of region is fw, don't register to nosave regions
		 */
		if (strcmp(regions[i].name, "fw")) {
			register_nosave_region_late(
					__phys_to_pfn(regions[i].start),
					__phys_to_pfn(regions[i].start +
						regions[i].size));
		}
	}
	return 0;
}
late_initcall(exynos_set_nosave_regions);
#endif /* CONFIG_HIBERNATION */
#endif /* CONFIG_S5P_MEM_CMA */

static void __init midas_map_io(void)
{
	clk_xusbxti.rate = 24000000;
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(slp_midas_uartcfgs, ARRAY_SIZE(slp_midas_uartcfgs));

#if defined(CONFIG_S5P_MEM_CMA)
	exynos4_reserve_mem();
#endif

	/* as soon as INFORM6 is visible, sec_debug is ready to run */
	sec_debug_init();
}

static void __init exynos_sysmmu_init(void)
{
	ASSIGN_SYSMMU_POWERDOMAIN(mfc_l, &exynos4_device_pd[PD_MFC].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(mfc_r, &exynos4_device_pd[PD_MFC].dev);
#ifdef CONFIG_DRM_EXYNOS_FIMD
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimd0).dev, &s5p_device_fimd0.dev);
#endif
#ifdef CONFIG_DRM_EXYNOS_HDMI
	sysmmu_set_owner(&SYSMMU_PLATDEV(tv).dev, &s5p_device_hdmi.dev);
#endif
#ifdef CONFIG_DRM_EXYNOS_G2D
	sysmmu_set_owner(&SYSMMU_PLATDEV(g2d_acp).dev, &s5p_device_fimg2d.dev);
#endif
#ifdef CONFIG_DRM_EXYNOS_ROTATOR
	sysmmu_set_owner(&SYSMMU_PLATDEV(rot).dev, &exynos_device_rotator.dev);
#endif
#ifdef CONFIG_DRM_EXYNOS_FIMC
	ASSIGN_SYSMMU_POWERDOMAIN(fimc2, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc3, &exynos4_device_pd[PD_CAM].dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc2).dev,
		&exynos_drm_fimc2_device.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc3).dev,
		&exynos_drm_fimc3_device.dev);
#endif
#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC) || defined(CONFIG_VIDEO_MFC5X)
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_l).dev, &s5p_device_mfc.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_r).dev, &s5p_device_mfc.dev);
#endif
}

/*
 * This function disable unused clocks to remove power leakage on idle state.
 */
static void midas_disable_unused_clock(void)
{
/* Following array include the unused clock list */
	struct __unused_clock_list {
		char *dev_id;
		char *con_id;
	} clock_list[] =  {
		{
			/* UART Ch 4 is only dedicated for communication
			 * with internal GPS in SoC */
			.dev_id = "s5pv210-uart.4",
			.con_id = "uart",
		}, {
			.dev_id = "s5p-qe.3",
			.con_id = "qefimc",
		}, {
			.dev_id = "s5p-qe.2",
			.con_id = "qefimc",
		}, {
			.dev_id = "s5p-qe.1",
			.con_id = "qefimc",
		},
	};
	struct device dev;
	struct clk *clk;
	char *con_id;
	int i;

	for (i = 0 ; i < ARRAY_SIZE(clock_list) ; i++) {
		dev.init_name = clock_list[i].dev_id;
		con_id = clock_list[i].con_id;

		clk = clk_get(&dev, con_id);
		if (IS_ERR(clk)) {
			printk(KERN_ERR "Failed to get %s for %s\n",
					con_id, dev.init_name);
			continue;
		}
		clk_enable(clk);
		clk_disable(clk);
		clk_put(clk);
	}
}

static void __init midas_machine_init(void)
{
#ifdef CONFIG_BUSFREQ_OPP
	struct clk *ppmu_clk = NULL;
#endif
#if defined(CONFIG_S3C64XX_DEV_SPI)
	unsigned int gpio;
	struct clk *sclk = NULL;
	struct clk *prnt = NULL;
	struct device *spi1_dev = &exynos_device_spi1.dev;
#endif
	strcpy(utsname()->nodename, machine_desc->name);

	/* Workaround: bootloader needs to set GPX*PUD registers */
	s3c_gpio_setpull(EXYNOS4_GPX2(7), S3C_GPIO_PULL_NONE);

#if defined(CONFIG_EXYNOS_DEV_PD) && defined(CONFIG_PM_RUNTIME)
	exynos_pd_disable(&exynos4_device_pd[PD_MFC].dev);

	/*
	 * FIXME: now runtime pm of mali driver isn't worked yet.
	 * if the runtime pm is worked fine, then remove this call.
	 */
	exynos_pd_enable(&exynos4_device_pd[PD_G3D].dev);

	/* PD_LCD0 : The child devie control LCD0 power domain
	 * because LCD should be always enabled during kernel booting.
	 * So, LCD power domain can't turn off when machine initialization.*/
	exynos_pd_disable(&exynos4_device_pd[PD_CAM].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_TV].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_GPS].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_GPS_ALIVE].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_ISP].dev);
#elif defined(CONFIG_EXYNOS_DEV_PD)
	/*
	 * These power domains should be always on
	 * without runtime pm support.
	 */
	exynos_pd_enable(&exynos4_device_pd[PD_MFC].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_G3D].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_LCD0].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_CAM].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_TV].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_GPS].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_GPS_ALIVE].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_ISP].dev);
#endif

	/* initialise the gpios */
	midas_config_gpio_table();
	exynos4_sleep_gpio_table_set = midas_config_sleep_gpio_table;

	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	/* LSM330DLC (Gyro & Accelerometer Sensor) */
	s3c_i2c1_set_platdata(NULL);
	lsm331dlc_gpio_init();
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	s3c_i2c3_set_platdata(NULL);

#ifdef CONFIG_MACH_SLP_PQ
	if (board_is_m0() && hwrevision(M0_PROXIMA_REV0_0)) {
		/* pq_proxima rev00 */
		GPIO_I2C_PIN_SETUP(codec);
		i2c_register_board_info(I2C_CODEC, i2c_devs_codec,
					ARRAY_SIZE(i2c_devs_codec));
		platform_device_register(&device_i2c_codec);
	} else {
		/* pq_proxima r2 and above */
		s3c_i2c4_set_platdata(NULL);
		i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
	}
#else
	/* CONFIG_MACH_SLP_PQ_LTE */
	GPIO_I2C_PIN_SETUP(codec);
	i2c_register_board_info(I2C_CODEC, i2c_devs_codec,
				ARRAY_SIZE(i2c_devs_codec));
	platform_device_register(&device_i2c_codec);
#endif
	s3c_i2c7_set_platdata(NULL);
	s3c_i2c7_set_platdata(NULL);

	/* Workaround for repeated interrupts from MAX77686 during sleep */
	if (board_is_m0() && hwrevision(M0_PROXIMA_REV0_0))
		exynos4_max77686_info.wakeup = 0;

	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));

	GPIO_I2C_PIN_SETUP(if_pmic);
	midas_power_set_muic_pdata(NULL, EXYNOS4_GPX0(7));

	i2c_if_pmic_register();

	/* NFC */
#ifdef CONFIG_MACH_SLP_PQ
	if (board_is_m0() && (hwrevision(M0_PROXIMA_REV0_1) ||
				system_rev >= M0_REAL_REV0_6)) {
		s3c_i2c5_set_platdata(NULL);
		platform_device_register(&s3c_device_i2c5);
		midas_nfc_init(s3c_device_i2c5.id);
	} else {
		GPIO_I2C_PIN_SETUP(nfc);
		platform_device_register(&device_i2c_nfc);
		midas_nfc_init(device_i2c_nfc.id);
	}
#else
	/* CONFIG_MACH_SLP_PQ_LTE */
	GPIO_I2C_PIN_SETUP(nfc);
	platform_device_register(&device_i2c_nfc);
	midas_nfc_init(device_i2c_nfc.id);
#endif

	/* MHL / MHL_D */
	GPIO_I2C_PIN_SETUP(mhl);

#ifdef CONFIG_MACH_SLP_PQ
	if (board_is_m0() && hwrevision(M0_PROXIMA_REV0_0)) {
		GPIO_I2C_PIN_SETUP(mhl_d);
		platform_device_register(&device_i2c_mhl_d);
	} else {
		/* nothing */
	}
#else
	GPIO_I2C_PIN_SETUP(mhl_d);
	platform_device_register(&device_i2c_mhl_d);
#endif

	if (board_is_m0()) {
		lps331ap_gpio_init();
		GPIO_I2C_PIN_SETUP(bsense);
		i2c_register_board_info(I2C_BSENSE, i2c_devs_bsense,
				ARRAY_SIZE(i2c_devs_bsense));
	}

#ifdef CONFIG_MACH_SLP_PQ
	msense_gpio_init();
#endif
	GPIO_I2C_PIN_SETUP(msense);

	if (board_is_redwood())
		i2c_register_board_info(I2C_MSENSE, i2c_devs_msense_ak8963,
				ARRAY_SIZE(i2c_devs_msense_ak8963));
	else
		i2c_register_board_info(I2C_MSENSE, i2c_devs_msense_ak8975,
				ARRAY_SIZE(i2c_devs_msense_ak8975));

	optical_gpio_init();

#ifdef CONFIG_MACH_SLP_PQ
	if (hwrevision(M0_PROXIMA_REV0_0) || hwrevision(M0_PROXIMA_REV0_1) ||
		(board_is_redwood() && !hwrevision(REDWOOD_REAL_REV0_0))) {
		if (board_is_redwood()) {
			gpio_i2c_psense_gp2a.sda_pin = GPIO_RGB_SDA_1_8V;
			gpio_i2c_psense_gp2a.scl_pin = GPIO_RGB_SCL_1_8V;
		}

		GPIO_I2C_PIN_SETUP(psense_gp2a);
		i2c_register_board_info(I2C_PSENSE, i2c_devs_psense_gp2a,
					ARRAY_SIZE(i2c_devs_psense_gp2a));

		platform_device_register(&device_i2c_psense_gp2a);
		platform_device_register(&opt_gp2a);
	} else {
		GPIO_I2C_PIN_SETUP(psense_cm36651);
		i2c_register_board_info(I2C_PSENSE, i2c_devs_psense_cm36651,
					ARRAY_SIZE(i2c_devs_psense_cm36651));

		platform_device_register(&device_i2c_psense_cm36651);
	}
#else /* CONFIG_MACH_SLP_PQ_LTE */
	GPIO_I2C_PIN_SETUP(psense_gp2a);
	i2c_register_board_info(I2C_PSENSE, i2c_devs_psense_gp2a,
				ARRAY_SIZE(i2c_devs_psense_gp2a));

	platform_device_register(&device_i2c_psense_gp2a);
	platform_device_register(&opt_gp2a);

#endif

#ifdef CONFIG_USB_EHCI_S5P
	smdk4212_ehci_init();
#endif
#ifdef CONFIG_USB_OHCI_S5P
	smdk4212_ohci_init();
#endif
#ifdef CONFIG_USB_GADGET
	smdk4212_usbgadget_init();
#endif

	GPIO_I2C_PIN_SETUP(3_touch);
	gpio_request(GPIO_3_TOUCH_INT, "3_TOUCH_INT");
	s5p_register_gpio_interrupt(GPIO_3_TOUCH_INT);
	i2c_register_board_info(I2C_3_TOUCH, i2c_devs_3_touch,
				ARRAY_SIZE(i2c_devs_3_touch));

	GPIO_I2C_PIN_SETUP(fuel);
	i2c_register_board_info(I2C_FUEL, i2c_devs_fuel,
				ARRAY_SIZE(i2c_devs_fuel));

#ifdef CONFIG_I2C_SI4705
	GPIO_I2C_PIN_SETUP(fm_radio);
	pq_si4705_init();
	i2c_register_board_info(I2C_FM_RADIO, i2c_devs_fm_radio,
				ARRAY_SIZE(i2c_devs_fm_radio));
#endif
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	exynos_dwmci_set_platdata(&exynos_dwmci_pdata, 0);
#else
	s3c_mshci_set_platdata(&exynos4_mshc_pdata);
#endif
	s3c_sdhci2_set_platdata(&slp_midas_hsmmc2_pdata);
	s3c_sdhci3_set_platdata(&slp_midas_hsmmc3_pdata);

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	if (system_rev >= 0x07)
		exynos4_fimc_is_set_platdata(&exynos4_fimc_is_plat);
	else
		exynos4_fimc_is_set_platdata(&exynos4_fimc_is_plat_old);
	exynos4_device_fimc_is.dev.parent = &exynos4_device_pd[PD_ISP].dev;
#endif

	/* FIMC */
	pq_camera_init();

#ifdef CONFIG_DRM_EXYNOS_FIMC
	midas_fimc_init();
#endif

#ifdef CONFIG_DRM_EXYNOS_FIMD
	/*
	 * platform device name for fimd driver should be changed
	 * because we can get source clock with this name.
	 *
	 * P.S. refer to sclk_fimd definition of clock-exynos4.c
	 */
	s5p_fb_setname(0, "s3cfb");
	s5p_device_fimd0.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#ifdef CONFIG_S5P_MIPI_DSI2
	s5p_device_mipi_dsim0.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif

	setup_charger_manager(&midas_charger_g_desc);

#ifdef CONFIG_VIDEO_JPEG_V2X
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_jpeg.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	exynos4_jpeg_setup_clock(&s5p_device_jpeg.dev, 160000000);
#endif
#endif

#ifdef CONFIG_EXYNOS4_SETUP_THERMAL
	s5p_tmu_set_platdata(&midas_tmu_data);
#endif

#ifdef CONFIG_SEC_THERMISTOR
	platform_device_register(&sec_device_thermistor);
#endif

#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	dev_set_name(&s5p_device_mfc.dev, "s3c-mfc");
	clk_add_alias("mfc", "s5p-mfc", "mfc", &s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc");
#endif

#if defined(CONFIG_VIDEO_MFC5X) || defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_mfc.dev.parent = &exynos4_device_pd[PD_MFC].dev;
#endif
	exynos4_mfc_setup_clock(&s5p_device_mfc.dev, 267 * MHZ);
#endif

	exynos_sysmmu_init();

	/* Disable unused clocks to remove power leakage on idle state */
	midas_disable_unused_clock();

#ifdef CONFIG_SENSORS_NTC_THERMISTOR
	/* PQ Rev00 doesn't have ntc on board */
	if (board_is_m0() && !hwrevision(M0_PROXIMA_REV0_0))
		adc_ntc_init(2); /* Channel 2 */
#endif
	/* Battery capacity has changed from 1750mA to 2100mA(rev06) */
	if (system_rev >= 0x7) {
		/* setting for top off voltage */
		max77693_change_top_off_vol();
#ifdef CONFIG_BATTERY_SAMSUNG
		/* setting for Battery Capacity */
		samsung_battery_pdata.voltage_max = 4350000;
		samsung_battery_pdata.recharge_voltage = 4300000;
#endif
#ifdef CONFIG_CHARGER_MANAGER
		cm_change_fullbatt_uV();
#endif
	}

#ifdef CONFIG_S3C_ADC
	if (system_rev != 3)
		platform_device_register(&s3c_device_adc);
#endif

	platform_add_devices(slp_midas_devices, ARRAY_SIZE(slp_midas_devices));

#ifdef CONFIG_DRM_EXYNOS_FIMD
	midas_fb_init();
#endif
#ifdef CONFIG_DRM_EXYNOS_HDMI
	midas_tv_init();
#endif

	brcm_wlan_init();
#if defined(CONFIG_S3C64XX_DEV_SPI)
	sclk = clk_get(spi1_dev, "dout_spi1");
	if (IS_ERR(sclk))
		dev_err(spi1_dev, "failed to get sclk for SPI-1\n");
	prnt = clk_get(spi1_dev, "mout_mpll_user");
	if (IS_ERR(prnt))
		dev_err(spi1_dev, "failed to get prnt\n");
	if (clk_set_parent(sclk, prnt))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
		       prnt->name, sclk->name);

	clk_set_rate(sclk, 800 * 1000 * 1000);
	clk_put(sclk);
	clk_put(prnt);

	if (!gpio_request(EXYNOS4_GPB(5), "SPI_CS1")) {
		gpio_direction_output(EXYNOS4_GPB(5), 1);
		s3c_gpio_cfgpin(EXYNOS4_GPB(5), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(EXYNOS4_GPB(5), S3C_GPIO_PULL_UP);
		exynos_spi_set_info(1, EXYNOS_SPI_SRCCLK_SCLK,
				     ARRAY_SIZE(spi1_csi));
	}

	for (gpio = EXYNOS4_GPB(4); gpio < EXYNOS4_GPB(8); gpio++)
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	spi_register_board_info(spi1_board_info, ARRAY_SIZE(spi1_board_info));
#endif
#ifdef CONFIG_BUSFREQ_OPP
	dev_add(&busfreq, &exynos4_busfreq.dev);

	/* PPMUs using for cpufreq get clk from clk_list */
	ppmu_clk = clk_get(NULL, "ppmudmc0");
	if (IS_ERR(ppmu_clk))
		printk(KERN_ERR "failed to get ppmu_dmc0\n");
	clk_enable(ppmu_clk);
	clk_put(ppmu_clk);

	ppmu_clk = clk_get(NULL, "ppmudmc1");
	if (IS_ERR(ppmu_clk))
		printk(KERN_ERR "failed to get ppmu_dmc1\n");
	clk_enable(ppmu_clk);
	clk_put(ppmu_clk);

	ppmu_clk = clk_get(NULL, "ppmucpu");
	if (IS_ERR(ppmu_clk))
		printk(KERN_ERR "failed to get ppmu_cpu\n");
	clk_enable(ppmu_clk);
	clk_put(ppmu_clk);

	ppmu_init(&exynos_ppmu[PPMU_DMC0], &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_DMC1], &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_CPU], &exynos4_busfreq.dev);
#endif

	check_hw_revision();
}

MACHINE_START(TRATS2, "TRATS2")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= exynos4_init_irq,
	.map_io		= midas_map_io,
	.init_machine	= midas_machine_init,
	.timer		= &exynos4_timer,
MACHINE_END
