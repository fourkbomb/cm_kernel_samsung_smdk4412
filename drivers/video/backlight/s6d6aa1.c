/* linux/drivers/video/backlight/s6d6aa1.c
 *
 * MIPI-DSI based s6d6aa1 TFT-LCD 4.77 inch panel driver.
 *
 * Joongmock Shin <jmock.shin@samsung.com>
 * Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/lcd.h>
#include <linux/lcd-property.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <video/mipi_display.h>
#include <linux/workqueue.h>

#include <plat/mipi_dsim2.h>
#include "s6d6aa1.h"

#define VER_16	(0x10)	/* MACH_SLP_REDWOORD */
#define LDI_FW_PATH "s6d6aa1/reg_%s.bin"
#define MAX_STR				255
#define MAX_READ_LENGTH		64
#define DIMMING_BRIGHTNESS	(0)
#define MIN_BRIGHTNESS		(1)
#define MAX_BRIGHTNESS		(100)
#define DSCTL_VFLIP	(1 << 7)
#define DSCTL_HFLIP	(1 << 6)
#ifdef CONFIG_BL_OVERHEATING_PROTECTION
#define OVERHEATING_LIMIT_BRIGHTNESS 69     /*limit brightness is 230cd */
#endif

#define POWER_IS_ON(pwr)		((pwr) == FB_BLANK_UNBLANK)
#define POWER_IS_OFF(pwr)	((pwr) == FB_BLANK_POWERDOWN)
#define POWER_IS_NRM(pwr)	((pwr) == FB_BLANK_NORMAL)

#define lcd_to_master(a)		(a->dsim_dev->master)
#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)

/* white magic mode */
enum wm_mode {
	WM_MODE_MIN = 0x00,
	WM_MODE_NORMAL = WM_MODE_MIN,
	WM_MODE_CONSERVATIVE,
	WM_MODE_MEDIUM,
	WM_MODE_AGGRESSIVE,
	WM_MODE_OUTDOOR,
	WM_MODE_MAX = WM_MODE_OUTDOOR
};

struct panel_model {
	int ver;
	char *name;
};

struct s6d6aa1 {
	struct device	*dev;
	struct lcd_device	*ld;
	struct backlight_device	*bd;
	struct mipi_dsim_lcd_device	*dsim_dev;
	struct lcd_platform_data	*ddi_pd;
	struct lcd_property	*property;

	struct regulator	*reg_vddi;
	struct regulator	*reg_vdd;

	struct mutex	lock;
	struct mutex	power_lock;

	unsigned int	ver;
	unsigned int	power;
	enum wm_mode	wm_mode;
	unsigned int	cur_addr;

	const struct panel_model	*model;
	unsigned int	model_count;
	struct work_struct		esd_work;
};

static void s6d6aa1_delay(unsigned int msecs)
{
	/* refer from documentation/timers/timers-howto.txt */
	if (msecs < 20)
		usleep_range(msecs*1000, (msecs+1)*1000);
	else
		msleep(msecs);
}

static void s6d6aa1_sleep_in(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE, 0x10, 0x00);
}

static void s6d6aa1_sleep_out(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE, 0x11, 0x00);
}

static void s6d6aa1_apply_level_1_key(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xF0, 0x5A, 0x5A
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_apply_level_2_key(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xF1, 0x5A, 0x5A
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_read_id(struct s6d6aa1 *lcd, u8 *mtp_id)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_read(lcd_to_master(lcd),
			MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM,
			0xDA, 1, &mtp_id[0]);
	ops->cmd_read(lcd_to_master(lcd),
			MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM,
			0xDB, 1, &mtp_id[1]);
	ops->cmd_read(lcd_to_master(lcd),
			MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM,
			0xDC, 1, &mtp_id[2]);
}

static void s6d6aa1_write_ddb(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xB4, 0x59, 0x10, 0x10, 0x00
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_bcm_mode(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0xC1, 0x03);
}

static void s6d6aa1_wrbl_ctl(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xC3, 0x7C, 0x00, 0x22
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_sony_ip_setting(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send1[] = {
		0xC4, 0x7C, 0xE6, 0x7C, 0xE6, 0x7C, 0xE6, 0x7C,
		0x7C, 0x05, 0x0F, 0x1F, 0x01, 0x00, 0x00,
	};
	const unsigned char data_to_send2[] = {
		0xC5, 0x80, 0x80, 0x80, 0x41, 0x43, 0x34,
		0x80, 0x80, 0x01, 0xFF, 0x25, 0x58, 0x50,
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send1, ARRAY_SIZE(data_to_send1));

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send2, ARRAY_SIZE(data_to_send2));
}

static void s6d6aa1_disp_ctl(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct lcd_property	*property = lcd->property;
	unsigned char cfg = 0;

	if (property) {
		if (property->flip & LCD_PROPERTY_FLIP_VERTICAL)
			cfg |= DSCTL_VFLIP;

		if (property->flip & LCD_PROPERTY_FLIP_HORIZONTAL)
			cfg |= DSCTL_HFLIP;
	}

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0x36, cfg);
}

static void s6d6aa1_source_ctl(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xF2, 0x03, 0x03, 0x91, 0x85
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_pwr_ctl(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xF4, 0x04, 0x0B, 0x07, 0x07, 0x10, 0x14, 0x0D, 0x0C,
		0xAD, 0x00, 0x33, 0x33
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_panel_ctl(struct s6d6aa1 *lcd, int high_freq)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xF6, 0x0B, 0x11, 0x0F, 0x25, 0x0A, 0x00, 0x13, 0x22,
		0x1B, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x03,
		0x12, 0x32, 0x51
	};

	/* ToDo : Low requency control */

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_mount_ctl(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0xF7, 0x00);
}

static int s6d6aa1_gamma_ctrl(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send1[] = {
		0xFA, 0x9C, 0xBF, 0x1A, 0xD6, 0xE3, 0xE3, 0x1B,
		0xDA, 0x9B, 0x16, 0x51, 0x12, 0x15, 0xD9, 0x9B,
		0x1A, 0xDD, 0x62, 0x2D, 0x79, 0x6A, 0x2C, 0x7F,
		0x11, 0x09, 0x53, 0x91, 0x09, 0x08, 0xCA, 0x06,
		0x07, 0x89, 0x0D, 0x52, 0xD5, 0x16, 0xD9, 0x1C,
		0x1E, 0x9E, 0xC1, 0x0F, 0xFF, 0xD7, 0x53, 0x61,
		0xE2, 0x5A, 0x9A, 0xDC, 0x5A, 0x97, 0x99, 0x5D,
		0x21, 0xE5, 0x26, 0xE9, 0x2C, 0x2A, 0x2A, 0x4F,
	};

	const unsigned char data_to_send2[] = {
		0xFB, 0x9C, 0xBF, 0x1A, 0xD6, 0xE3, 0xE3, 0x1B,
		0xDA, 0x9B, 0x16, 0x51, 0x12, 0x15, 0xD9, 0x9B,
		0x1A, 0xDD, 0x62, 0x2D, 0x79, 0x6A, 0x2C, 0x7F,
		0x11, 0x09, 0x53, 0x91, 0x09, 0x08, 0xCA, 0x06,
		0x07, 0x89, 0x0D, 0x52, 0xD5, 0x16, 0xD9, 0x1C,
		0x1E, 0x9E, 0xC1, 0x0F, 0xFF, 0xD7, 0x53, 0x61,
		0xE2, 0x5A, 0x9A, 0xDC, 0x5A, 0x97, 0x99, 0x5D,
		0x21, 0xE5, 0x26, 0xE9, 0x2C, 0x2A, 0x2A, 0x4F,
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send1, ARRAY_SIZE(data_to_send1));

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send2, ARRAY_SIZE(data_to_send2));

	return 0;
}

static int s6d6aa1_panel_init(struct s6d6aa1 *lcd)
{
	s6d6aa1_sleep_out(lcd);
	s6d6aa1_delay(140);
/*
 * FIXME:!!simple init vs full init
 * If lcd don't working simple sequence,
 * we use full initialization sequence
 */
#ifdef SIMPLE_INIT
	s6d6aa1_disp_ctl(lcd);
#else
	s6d6aa1_apply_level_1_key(lcd);
	s6d6aa1_apply_level_2_key(lcd);

	s6d6aa1_write_ddb(lcd);
	s6d6aa1_bcm_mode(lcd);
	s6d6aa1_wrbl_ctl(lcd);
	s6d6aa1_sony_ip_setting(lcd);
	s6d6aa1_disp_ctl(lcd);
	s6d6aa1_source_ctl(lcd);
	s6d6aa1_pwr_ctl(lcd);
	s6d6aa1_panel_ctl(lcd, 1);
	s6d6aa1_mount_ctl(lcd);

	s6d6aa1_gamma_ctrl(lcd);

	/* wait more than 10ms */
	s6d6aa1_delay(lcd->ddi_pd->power_on_delay);
#endif

	return 0;
}

static void s6d6aa1_write_disbv(struct s6d6aa1 *lcd,
				unsigned int brightness)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0x51, brightness);
}

static void s6d6aa1_write_ctrld(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0x53, 0x2C);
}

static void s6d6aa1_write_cabc(struct s6d6aa1 *lcd,
			enum wm_mode wm_mode)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0x55, wm_mode);
}

static void s6d6aa1_display_on(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE, 0x29, 0x00);
}

static void s6d6aa1_display_off(struct s6d6aa1 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE, 0x28, 0x00);
}

static int s6d6aa1_early_set_power(struct lcd_device *ld, int power)
{
	struct s6d6aa1 *lcd = lcd_get_data(ld);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	int ret = 0;

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
			power != FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	if (lcd->power == power) {
		dev_err(lcd->dev, "power mode is same as previous one.\n");
		return -EINVAL;
	}

	if (ops->set_early_blank_mode) {
		/* LCD power off */
		if ((POWER_IS_OFF(power) && POWER_IS_ON(lcd->power))
			|| (POWER_IS_ON(lcd->power) && POWER_IS_NRM(power))) {
			ret = ops->set_early_blank_mode(lcd_to_master(lcd),
							power);
			if (!ret && lcd->power != power)
				lcd->power = power;
		}
	}

	return ret;
}

static int s6d6aa1_set_power(struct lcd_device *ld, int power)
{
	struct s6d6aa1 *lcd = lcd_get_data(ld);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	int ret = 0;

	mutex_lock(&lcd->power_lock);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
			power != FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "power value should be 0, 1 or 4.\n");
		ret = -EINVAL;
		goto err_unlock;
	}

	if (lcd->power == power) {
		dev_err(lcd->dev, "power mode is same as previous one.\n");
		ret = -EINVAL;
		goto err_unlock;
	}

	if (ops->set_blank_mode) {
		ret = ops->set_blank_mode(lcd_to_master(lcd), power);
		if (!ret && lcd->power != power)
			lcd->power = power;
	}

err_unlock:
	mutex_unlock(&lcd->power_lock);

	return ret;
}

static int s6d6aa1_get_power(struct lcd_device *ld)
{
	struct s6d6aa1 *lcd = lcd_get_data(ld);

	return lcd->power;
}

static struct lcd_ops s6d6aa1_lcd_ops = {
	.early_set_power = s6d6aa1_early_set_power,
	.set_power = s6d6aa1_set_power,
	.get_power = s6d6aa1_get_power,
};

static int s6d6aa1_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int s6d6aa1_get_minbrightness(struct backlight_device *bd)
{
	return MIN_BRIGHTNESS;
}

static void s6d6aa1_brightness_ctrl(struct s6d6aa1 *lcd,
				unsigned int brightness)
{
	const unsigned int convert_table[] = {
		8, 9, 11, 13, 15, 17, 19, 21, 23, 25,
		27, 29, 31, 33, 35, 37, 39, 41, 43, 45,
		47, 49, 51, 53, 55, 57, 59, 61, 63, 65,
		67, 69, 71, 73, 75, 77, 79, 81, 83, 85,
		87, 89, 91, 93, 95, 97, 99, 101, 103, 105,
		107, 109, 111, 113, 115, 117, 119, 121, 123, 125,
		127, 130, 133, 136, 139, 142, 145, 149, 152, 155,
		158, 161, 164, 167, 170, 173, 176, 179, 182, 185,
		189, 192, 195, 198, 201, 204, 207, 210, 213, 216,
		219, 222, 225, 228, 232, 235, 238, 241, 244, 247,
		250,
	};

	if (brightness > ARRAY_SIZE(convert_table)-1)
		brightness = convert_table[ARRAY_SIZE(convert_table)-1];
	else
		brightness = convert_table[brightness];

	s6d6aa1_write_disbv(lcd, brightness);
}


static int s6d6aa1_set_brightness(struct backlight_device *bd)
{
	unsigned int  brightness = bd->props.brightness;
	struct s6d6aa1 *lcd = bl_get_data(bd);

	if (lcd->power == FB_BLANK_POWERDOWN) {
		dev_err(lcd->dev,
			"lcd off: brightness set failed.\n");
		return -EINVAL;
	}

	if (brightness < 0 || brightness > bd->props.max_brightness) {
		dev_err(lcd->dev, "lcd brightness should be %d to %d.\n",
			0, MAX_BRIGHTNESS);
		return -EINVAL;
	}

#ifdef CONFIG_BL_OVERHEATING_PROTECTION
	if ((bd->props.overheating) &&
	    (brightness > OVERHEATING_LIMIT_BRIGHTNESS)) {
		dev_err(lcd->dev, "overheating condition: brightness set failed.\n");
		return -EINVAL;
	}
#endif

	s6d6aa1_brightness_ctrl(lcd, brightness);
	return 0;
}

static int s6d6aa1_set_dimming(struct backlight_device *bd)
{
	struct s6d6aa1 *lcd = bl_get_data(bd);
	int brightness = bd->props.brightness;

	if (lcd->power == FB_BLANK_POWERDOWN) {
		dev_err(lcd->dev,
			"lcd dimming: dimming set failed.\n");
		return -EINVAL;
	}

	dev_info(lcd->dev, "%s: brightness %d, dimming %d.\n",
		__func__, brightness, bd->props.dimming);

	if (bd->props.dimming)
		s6d6aa1_brightness_ctrl(lcd, DIMMING_BRIGHTNESS);
	else
		s6d6aa1_brightness_ctrl(lcd, brightness);

	return 0;
}

#ifdef CONFIG_BL_OVERHEATING_PROTECTION
static int s6d6aa1_set_overheating_protection(struct backlight_device *bd)
{
	struct s6d6aa1 *lcd = bl_get_data(bd);
	int brightness = bd->props.brightness;

	if (lcd->power == FB_BLANK_POWERDOWN) {
		dev_err(lcd->dev,
			"lcd overheating: overheating set failed.\n");
		return -EINVAL;
	}

	dev_info(lcd->dev, "%s: brightness %d overheating %d.\n",
		__func__, brightness, bd->props.overheating);

	if (bd->props.overheating) {
		if (brightness > OVERHEATING_LIMIT_BRIGHTNESS)
			s6d6aa1_brightness_ctrl
			    (lcd, OVERHEATING_LIMIT_BRIGHTNESS);
	} else
		s6d6aa1_brightness_ctrl(lcd, brightness);

	return 0;
}
#endif

static const struct backlight_ops s6d6aa1_backlight_ops = {
	.get_brightness = s6d6aa1_get_brightness,
	.update_status = s6d6aa1_set_brightness,
	.set_dimming = s6d6aa1_set_dimming,
	.get_minbrightness = s6d6aa1_get_minbrightness,
#ifdef CONFIG_BL_OVERHEATING_PROTECTION
	.set_overheating_protection = s6d6aa1_set_overheating_protection,
#endif
};

static ssize_t wm_mode_show(struct device *dev, struct
	device_attribute * attr, char *buf)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->wm_mode);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t wm_mode_store(struct device *dev, struct
	device_attribute * attr, const char *buf, size_t size)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (value < WM_MODE_MIN || value > WM_MODE_MAX) {
		dev_info(dev, "failed to set white magic mode to %d\n",
						value);
		return -EINVAL;
	}

	if (lcd->wm_mode != value) {
		dev_info(dev, "wm mode changed from %d to %d\n",
						lcd->wm_mode, value);
		lcd->wm_mode = value;
		s6d6aa1_write_cabc(lcd, lcd->wm_mode);
	}
	return size;
}

static ssize_t lcd_type_show(struct device *dev, struct
	device_attribute * attr, char *buf)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(dev);
	char temp[32];
	int i;

	for (i = 0; i < lcd->model_count; i++) {
		if (lcd->ver == lcd->model[i].ver)
			break;
	}

	if (i == lcd->model_count)
		return -EINVAL;

	sprintf(temp, "%s\n", lcd->model[i].name);
	strcpy(buf, temp);

	return strlen(buf);
}

static int s6d6aa1_read_reg(struct s6d6aa1 *lcd, unsigned int addr, char *buf)
{
	unsigned char data[MAX_READ_LENGTH];
	unsigned int size;
	int i;
	int pos = 0;
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	memset(data, 0x0, ARRAY_SIZE(data));
	size = ops->cmd_read(lcd_to_master(lcd),
			MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM,
			addr, MAX_READ_LENGTH, data);
	if (!size) {
		dev_err(lcd->dev, "failed to read 0x%.2x register.\n", addr);
		return size;
	}

	pos += sprintf(buf, "0x%.2x, ", addr);
	for (i = 1; i < size+1; i++) {
		if (i % 9 == 0)
			pos += sprintf(buf+pos, "\n");
		pos += sprintf(buf+pos, "0x%.2x, ", data[i-1]);
	}
	pos += sprintf(buf+pos, "\n");

	return pos;
}

static int s6d6aa1_write_reg(struct s6d6aa1 *lcd, char *name)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const struct firmware *fw;
	char fw_path[MAX_STR+1];
	int ret = 0;

	mutex_lock(&lcd->lock);
	snprintf(fw_path, MAX_STR, LDI_FW_PATH, name);

	ret = request_firmware(&fw, fw_path, lcd->dev);
	if (ret) {
		dev_err(lcd->dev, "failed to request firmware.\n");
		mutex_unlock(&lcd->lock);
		return ret;
	}

	if (fw->size == 1)
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_DCS_SHORT_WRITE,
				(unsigned int)fw->data[0], 0);
	else if (fw->size == 2)
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				(unsigned int)fw->data[0], fw->data[1]);
	else
		ret = ops->cmd_write(lcd_to_master(lcd),
				MIPI_DSI_DCS_LONG_WRITE,
				(unsigned int)fw->data, fw->size);
	if (ret)
		dev_err(lcd->dev, "failed to write 0x%.2x register and %d error.\n",
			fw->data[0], ret);

	release_firmware(fw);
	mutex_unlock(&lcd->lock);

	return ret;
}

static ssize_t read_reg_show(struct device *dev, struct
	device_attribute * attr, char *buf)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(dev);

	if (lcd->cur_addr == 0) {
		dev_err(dev, "failed to set current lcd register.\n");
		return -EINVAL;
	}

	return s6d6aa1_read_reg(lcd, lcd->cur_addr, buf);
}

static ssize_t read_reg_store(struct device *dev, struct
	device_attribute * attr, const char *buf, size_t size)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = sscanf(buf, "0x%x", &value);
	if (ret < 0)
		return ret;

	dev_info(dev, "success to set 0x%x address.\n", value);
	lcd->cur_addr = value;

	return size;
}

static ssize_t write_reg_store(struct device *dev, struct
	device_attribute * attr, const char *buf, size_t size)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(dev);
	char name[32];
	int ret;

	ret = sscanf(buf, "%s", name);
	if (ret < 0)
		return ret;

	ret = s6d6aa1_write_reg(lcd, name);
	if (ret < 0)
		return ret;

	dev_info(dev, "success to set %s address.\n", name);

	return size;
}

static ssize_t backlight_show_min_brightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", MIN_BRIGHTNESS);
}

static struct device_attribute ld_device_attrs[] = {
	__ATTR(wm_mode, S_IRUGO|S_IWUSR|S_IWGRP,
			wm_mode_show, wm_mode_store),
	__ATTR(lcd_type, S_IRUGO,
			lcd_type_show, NULL),
	__ATTR(read_reg, S_IRUGO|S_IWUSR|S_IWGRP,
			read_reg_show, read_reg_store),
	__ATTR(write_reg, S_IWUSR|S_IWGRP,
			NULL, write_reg_store),
};

static struct panel_model s6d6aa1_model[] = {
	{
		.ver = VER_16,			/* MACH_SLP_REDWOORD */
		.name = "JDI_ACX445BLN-7",
	}
};

static void s6d6aa1_regulator_ctl(struct s6d6aa1 *lcd, bool enable)
{
	mutex_lock(&lcd->lock);

	if (enable) {
		if (lcd->reg_vddi)
			regulator_enable(lcd->reg_vddi);

		if (lcd->reg_vdd)
			regulator_enable(lcd->reg_vdd);
	} else {
		if (lcd->reg_vdd)
			regulator_disable(lcd->reg_vdd);

		if (lcd->reg_vddi)
			regulator_disable(lcd->reg_vddi);
	}

	mutex_unlock(&lcd->lock);
}

static void s6d6aa1_power_on(struct mipi_dsim_lcd_device *dsim_dev,
				unsigned int enable)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(&dsim_dev->dev);

	dev_dbg(lcd->dev, "%s:enable[%d]\n", __func__, enable);

	if (enable) {
		/* lcd power on */
		s6d6aa1_regulator_ctl(lcd, true);

		s6d6aa1_delay(lcd->ddi_pd->reset_delay);

		/* lcd reset high */
		if (lcd->ddi_pd->reset)
			lcd->ddi_pd->reset(lcd->ld);

		/* wait more than 10ms */
		s6d6aa1_delay(lcd->ddi_pd->power_on_delay);
	} else {
		/* lcd reset low */
		if (lcd->property->reset_low)
			lcd->property->reset_low();

		/* lcd power off */
		s6d6aa1_regulator_ctl(lcd, false);
	}
}

static int s6d6aa1_check_mtp(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(&dsim_dev->dev);
	u8 mtp_id[3] = {0, };

	s6d6aa1_read_id(lcd, mtp_id);
	if (mtp_id[0] == 0x00) {
		dev_err(lcd->dev, "read id failed\n");
		return -EIO;
	}

	lcd->ver = mtp_id[1];
	dev_info(lcd->dev,
		"Read ID : %x, %x, %x\n", mtp_id[0], mtp_id[1], mtp_id[2]);

	return 0;
}

static void s6d6aa1_set_sequence(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct backlight_device *bd = lcd->bd;
	int brightness = bd->props.brightness;

	s6d6aa1_panel_init(lcd);
	s6d6aa1_brightness_ctrl(lcd, brightness);
	s6d6aa1_write_ctrld(lcd);
	s6d6aa1_write_cabc(lcd, lcd->wm_mode);
	s6d6aa1_display_on(lcd);
}

static void s6d6aa1_esd_work(struct work_struct *work)
{
	struct s6d6aa1 *lcd =
		container_of(work, struct s6d6aa1, esd_work);

	struct lcd_property *property = lcd->property;

	property->esd_level = property->get_esd_level();

	dev_info(lcd->dev, "%s:esd_level[%d]\n", __func__, property->esd_level);

	if (!property->esd_level) {
		s6d6aa1_set_power(lcd->ld, FB_BLANK_POWERDOWN);
		s6d6aa1_set_power(lcd->ld, FB_BLANK_UNBLANK);
	}
}

static irqreturn_t s6d6aa1_esd_handler(int irq, void *data)
{
	struct s6d6aa1 *lcd = data;

	if (!work_busy(&lcd->esd_work)) {
		schedule_work(&lcd->esd_work);
		dev_info(lcd->dev, "%s: add schedule_work.\n", __func__);
	}

	return IRQ_HANDLED;
}

static int s6d6aa1_probe(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6d6aa1 *lcd;
	struct lcd_property	*property;
	int ret = 0;
	int i;

	lcd = kzalloc(sizeof(struct s6d6aa1), GFP_KERNEL);
	if (!lcd) {
		dev_err(&dsim_dev->dev, "failed to allocate s6d6aa1 structure.\n");
		return -ENOMEM;
	}

	lcd->dsim_dev = dsim_dev;
	lcd->ddi_pd = (struct lcd_platform_data *)dsim_dev->platform_data;
	lcd->dev = &dsim_dev->dev;

	mutex_init(&lcd->lock);
	mutex_init(&lcd->power_lock);

	lcd->reg_vddi = regulator_get(lcd->dev, "VDDI");
	if (IS_ERR(lcd->reg_vddi)) {
		ret = PTR_ERR(lcd->reg_vddi);
		dev_err(lcd->dev, "failed to get %s regulator (%d)\n",
				"VDDI", ret);
		lcd->reg_vddi = NULL;
	}

	lcd->reg_vdd = regulator_get(lcd->dev, "VDD");
	if (IS_ERR(lcd->reg_vdd)) {
		ret = PTR_ERR(lcd->reg_vdd);
		dev_err(lcd->dev, "failed to get %s regulator (%d)\n",
				"VDD", ret);
		lcd->reg_vdd = NULL;
	}

	lcd->ld = lcd_device_register("s6d6aa1", lcd->dev, lcd,
			&s6d6aa1_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		ret = PTR_ERR(lcd->ld);
		goto err_regulator;
	}

	lcd->bd = backlight_device_register("s6d6aa1-bl", lcd->dev, lcd,
			&s6d6aa1_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dev_err(lcd->dev, "failed to register backlight ops.\n");
		ret = PTR_ERR(lcd->bd);
		goto err_unregister_lcd;
	}

	s6d6aa1_regulator_ctl(lcd, true);

	if (lcd->ddi_pd) {
		lcd->property = lcd->ddi_pd->pdata;
		property = lcd->property;

		INIT_WORK(&lcd->esd_work, s6d6aa1_esd_work);

		if (request_threaded_irq(property->esd_irq, NULL,
		    s6d6aa1_esd_handler, IRQF_TRIGGER_FALLING
		    | IRQF_ONESHOT, "esd", (void *) lcd)) {
			dev_err(&lcd->ld->dev, "failed to reqeust LCD_DET irq.\n");
			goto err_unregister_lcd;
		}
	}

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = MAX_BRIGHTNESS;
	lcd->power = FB_BLANK_UNBLANK;
	lcd->wm_mode = WM_MODE_NORMAL;
	lcd->model = s6d6aa1_model;
	lcd->model_count = ARRAY_SIZE(s6d6aa1_model);

	for (i = 0; i < ARRAY_SIZE(ld_device_attrs); i++) {
		ret = device_create_file(&lcd->ld->dev,
					&ld_device_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "failed to add sysfs entries\n");
			break;
		}
	}

	dev_set_drvdata(&dsim_dev->dev, lcd);
	dev_info(lcd->dev, "probed s6d6aa1 panel driver(%s).\n",
			dev_name(&lcd->ld->dev));

	return 0;

err_unregister_lcd:
	lcd_device_unregister(lcd->ld);

err_regulator:
	regulator_put(lcd->reg_vdd);
	regulator_put(lcd->reg_vddi);

	kfree(lcd);

	return ret;
}

static void s6d6aa1_remove(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(&dsim_dev->dev);

	backlight_device_unregister(lcd->bd);
	lcd_device_unregister(lcd->ld);

	regulator_put(lcd->reg_vdd);
	regulator_put(lcd->reg_vddi);

	kfree(lcd);
}

#ifdef CONFIG_PM
static int s6d6aa1_suspend(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct lcd_property *property = lcd->property;

	disable_irq(property->esd_irq);
	property->esd_gpio_suspend();

	s6d6aa1_display_off(lcd);
	s6d6aa1_sleep_in(lcd);
	s6d6aa1_delay(lcd->ddi_pd->power_off_delay);

	return 0;
}

static int s6d6aa1_resume(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct lcd_property *property = lcd->property;

	s6d6aa1_delay(lcd->ddi_pd->power_on_delay);

	property->esd_gpio_resume();
	enable_irq(property->esd_irq);

	return 0;
}
#else
#define s6d6aa1_suspend		NULL
#define s6d6aa1_resume		NULL
#endif

static struct mipi_dsim_lcd_driver s6d6aa1_dsim_ddi_driver = {
	.name = "s6d6aa1",
	.id = -1,

	.power_on = s6d6aa1_power_on,
	.check_mtp = s6d6aa1_check_mtp,
	.set_sequence = s6d6aa1_set_sequence,
	.probe = s6d6aa1_probe,
	.remove = s6d6aa1_remove,
	.suspend = s6d6aa1_suspend,
	.resume = s6d6aa1_resume,
};

static int s6d6aa1_init(void)
{
	s5p_mipi_dsi_register_lcd_driver(&s6d6aa1_dsim_ddi_driver);

	return 0;
}

static void s6d6aa1_exit(void)
{
	return;
}

module_init(s6d6aa1_init);
module_exit(s6d6aa1_exit);


MODULE_AUTHOR("Joongmock Shin <jmock.shin@samsung.com>");
MODULE_AUTHOR("Eunchul Kim <chulspro.kim@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based s6d6aa1 TFT-LCD Panel Driver");
MODULE_LICENSE("GPL");

