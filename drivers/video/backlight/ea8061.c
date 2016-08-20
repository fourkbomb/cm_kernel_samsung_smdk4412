/* linux/drivers/video/backlight/ea8061.c
 *
 * MIPI-DSI based ea8061 AMOLED lcd 5.55 inch panel driver.
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

#include <plat/mipi_dsim2.h>
#include "ea8061.h"

#include "ea8061_gamma.h"
#ifdef CONFIG_BACKLIGHT_SMART_DIMMING
#include "smart_dimming.h"
#endif

#define VER_161	(0xA1)	/* MACH_SLP_T0_LTE */
#define LDI_FW_PATH "ea8061/reg_%s.bin"
#define MAX_STR				255
#define LDI_MTP_LENGTH		24
#define MAX_READ_LENGTH		64
#define DIMMING_BRIGHTNESS	(0)
#define MIN_BRIGHTNESS		(1)
#define MAX_BRIGHTNESS		(100)

#define POWER_IS_ON(pwr)		((pwr) == FB_BLANK_UNBLANK)
#define POWER_IS_OFF(pwr)	((pwr) == FB_BLANK_POWERDOWN)
#define POWER_IS_NRM(pwr)	((pwr) == FB_BLANK_NORMAL)

#define lcd_to_master(a)		(a->dsim_dev->master)
#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)

struct panel_model {
	int ver;
	char *name;
};

struct ea8061 {
	struct device	*dev;
	struct lcd_device	*ld;
	struct backlight_device	*bd;
	struct mipi_dsim_lcd_device	*dsim_dev;
	struct lcd_platform_data	*ddi_pd;
	struct lcd_property	*property;

	struct regulator	*reg_vdd3;
	struct regulator	*reg_vci;

	struct mutex	lock;

	unsigned int	id;
	unsigned int	aid;
	unsigned int	ver;
	unsigned int	power;
	unsigned int	acl_enable;
	unsigned int	cur_addr;

	const struct panel_model	*model;
	unsigned int	model_count;

#ifdef CONFIG_BACKLIGHT_SMART_DIMMING
	unsigned int	support_elvss;
	struct str_smart_dim	smart_dim;
#endif
};

static void ea8061_delay(unsigned int msecs)
{
	/* refer from documentation/timers/timers-howto.txt */
	if (msecs < 20)
		usleep_range(msecs*1000, (msecs+1)*1000);
	else
		msleep(msecs);
}

static void ea8061_sleep_in(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE, 0x10, 0x00);
}

static void ea8061_sleep_out(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE, 0x11, 0x00);
}

static void ea8061_apply_level_1_key(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xF0, 0x5A, 0x5A
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void ea8061_apply_level_2_key(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xFC, 0x5A, 0x5A
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void ea8061_acl_on(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	/* FIXME: off, 33%, 40%, 50% */
	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0x55, 0x00);
}

static void ea8061_acl_off(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	/* FIXME: off, 33%, 40%, 50% */
	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0x55, 0x00);
}

static void ea8061_enable_mtp_register(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xF1, 0x5A, 0x5A
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void ea8061_disable_mtp_register(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xF1, 0xA5, 0xA5
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void ea8061_read_id(struct ea8061 *lcd, u8 *mtp_id)
{
	unsigned int retry_count = 3;
	unsigned int ret;
	unsigned int addr = 0xD1;	/* MTP ID */
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
			MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0xFD, addr);

	/* FIXME: First read always failed */
	do {
		ret = ops->cmd_read(lcd_to_master(lcd),
					MIPI_DSI_DCS_READ,
					0xFE, 3, mtp_id);
		if (!ret) {
			dev_info(lcd->dev, "ea8061_read_id fail = %d.\n",
					retry_count);
			retry_count--;
		} else
			break;
	} while (retry_count);
}

static unsigned int ea8061_read_mtp(struct ea8061 *lcd, u8 *mtp_data)
{
	unsigned int ret;
	unsigned int addr = 0xD3;	/* MTP addr */
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ea8061_enable_mtp_register(lcd);

	ret = ops->cmd_read(lcd_to_master(lcd),
			MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM,
			addr, LDI_MTP_LENGTH, mtp_data);

	ea8061_disable_mtp_register(lcd);

	return ret;
}

static void ea8061_disp_cond(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0x36, 0x02);
}

static void ea8061_panel_cond(struct ea8061 *lcd, int high_freq)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send[] = {
		0xc4, 0x4E, 0xBD, 0x00, 0x00, 0x58, 0xA7, 0x0B, 0x34,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x92, 0x0B, 0x92,
		0x08, 0x08, 0x07, 0x30, 0x50, 0x30, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x02, 0x04, 0x04
	};

	/* ToDo : Low requency control */

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

unsigned int convert_brightness_to_gamma(int brightness)
{
	const unsigned int gamma_table[] = {
		20, 30, 50, 70, 80, 90, 100, 120, 130, 140,
		150, 160, 170, 180, 190, 200, 210, 220, 230,
		240, 250, 260, 270, 280, 300
	};

	return gamma_table[brightness] - 1;
}

static int ea8061_gamma_ctrl(struct ea8061 *lcd, int brightness)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	const unsigned char data_to_send1[] = {
		0xF7, 0x5A, 0x5A
	};
	const unsigned char data_to_send2[] = {
		0xF7, 0xA5, 0xA5
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send1, ARRAY_SIZE(data_to_send1));

#ifdef CONFIG_BACKLIGHT_SMART_DIMMING
	unsigned int gamma;
	unsigned char gamma_set[GAMMA_TABLE_COUNT] = {0,};
	gamma = convert_brightness_to_gamma(brightness);

	gamma_set[0] = 0xfa;
	gamma_set[1] = 0x01;

	calc_gamma_table(&lcd->smart_dim, gamma, gamma_set + 2);

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)gamma_set,
			GAMMA_TABLE_COUNT);
#else
	/* FIXME: clean up after enable smart dimming */
	if (system_rev == 0x8) {
		ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)ea8061_gamma22_table_revD[brightness],
			GAMMA_TABLE_COUNT);
	} else {
		ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)ea8061_gamma22_table_revA[brightness],
			GAMMA_TABLE_COUNT);
	}
#endif
	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send2, ARRAY_SIZE(data_to_send2));

	ea8061_acl_on(lcd);

	return 0;
}

static int ea8061_brightness_ctrl(struct ea8061 *lcd, int brightness)
{
	const unsigned int convert_table[] = {
		0, 1, 1, 1, 2, 2, 2, 2, 3, 3,
		3, 3, 4, 4, 4, 4, 5, 5, 5, 5,
		6, 6, 6, 6, 7, 7, 7, 7, 7, 8,
		8, 8, 8, 9, 9, 9, 9, 10, 10, 10,
		10, 11, 11, 11, 11, 12, 12, 12, 12, 13,
		13, 13, 13, 13, 14, 14, 14, 14, 15, 15,
		15, 15, 16, 16, 16, 16, 17, 17, 17, 17,
		18, 18, 18, 18, 19, 19, 19, 19, 19, 20,
		20, 20, 20, 21, 21, 21, 21, 22, 22, 22,
		22, 23, 23, 23, 23, 24, 24, 24, 24, 24,
		24,
	};

	if (brightness > ARRAY_SIZE(convert_table)-1)
		brightness = convert_table[ARRAY_SIZE(convert_table)-1];
	else
		brightness = convert_table[brightness];

	return ea8061_gamma_ctrl(lcd, brightness);
}

static void ea8061_elvss_nvm_set(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	unsigned char data_to_send[] = {
		0xB2, 0x07, 0xB4, 0xA0, 0x00, 0x00, 0x00, 0x00
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void ea8061_slew_ctl(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	unsigned char data_to_send[] = {
		0xB4, 0x33, 0x0D, 0x00
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static void ea8061_ltps_aid(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	unsigned char data_to_send[] = {
		0xB3, 0x00, 0x0A, 0x00, 0x0A,
	};

	ops->cmd_write(lcd_to_master(lcd), MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)data_to_send, ARRAY_SIZE(data_to_send));
}

static int ea8061_panel_init(struct ea8061 *lcd)
{
	struct backlight_device *bd = lcd->bd;
	int brightness = bd->props.brightness;

	ea8061_delay(5);
	ea8061_apply_level_1_key(lcd);
	ea8061_apply_level_2_key(lcd);

	ea8061_panel_cond(lcd, 1);
	ea8061_disp_cond(lcd);
	ea8061_brightness_ctrl(lcd, brightness);
	ea8061_ltps_aid(lcd);
	ea8061_elvss_nvm_set(lcd);
	ea8061_acl_on(lcd);
	ea8061_slew_ctl(lcd);

	ea8061_sleep_out(lcd);

	/* wait more than 120ms */
	ea8061_delay(lcd->ddi_pd->power_on_delay);
	dev_info(lcd->dev, "panel init sequence done.\n");

	return 0;
}

static void ea8061_display_on(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE, 0x29, 0x00);
}

static void ea8061_display_off(struct ea8061 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);

	ops->cmd_write(lcd_to_master(lcd),
		MIPI_DSI_DCS_SHORT_WRITE, 0x28, 0x00);
}

static int ea8061_early_set_power(struct lcd_device *ld, int power)
{
	struct ea8061 *lcd = lcd_get_data(ld);
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

static int ea8061_set_power(struct lcd_device *ld, int power)
{
	struct ea8061 *lcd = lcd_get_data(ld);
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

	if (ops->set_blank_mode) {
		ret = ops->set_blank_mode(lcd_to_master(lcd), power);
		if (!ret && lcd->power != power)
			lcd->power = power;
	}

	return ret;
}

static int ea8061_get_power(struct lcd_device *ld)
{
	struct ea8061 *lcd = lcd_get_data(ld);

	return lcd->power;
}

static struct lcd_ops ea8061_lcd_ops = {
	.early_set_power = ea8061_early_set_power,
	.set_power = ea8061_set_power,
	.get_power = ea8061_get_power,
};

static int ea8061_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int ea8061_set_brightness(struct backlight_device *bd)
{
	int ret, brightness = bd->props.brightness;
	struct ea8061 *lcd = bl_get_data(bd);

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

	ret = ea8061_brightness_ctrl(lcd, brightness);
	if (ret) {
		dev_err(&bd->dev, "lcd brightness setting failed.\n");
		return -EIO;
	}

	return ret;
}

static int ea8061_set_dimming(struct backlight_device *bd)
{
	struct ea8061 *lcd = bl_get_data(bd);
	int ret = 0, brightness = bd->props.brightness;

	if (lcd->power == FB_BLANK_POWERDOWN) {
		dev_err(lcd->dev,
			"lcd dimming: dimming set failed.\n");
		return -EINVAL;
	}

	dev_info(lcd->dev, "%s: brightness %d, dimming %d.\n",
		__func__, brightness, bd->props.dimming);

	if (bd->props.dimming)
		ret = ea8061_brightness_ctrl(lcd, DIMMING_BRIGHTNESS);
	else
		ret = ea8061_brightness_ctrl(lcd, brightness);

	if (ret)
		dev_err(&bd->dev, "lcd dimming setting failed.\n");
		return -EIO;

	return ret;
}

static const struct backlight_ops ea8061_backlight_ops = {
	.get_brightness = ea8061_get_brightness,
	.update_status = ea8061_set_brightness,
	.set_dimming = ea8061_set_dimming,
};

static ssize_t acl_control_show(struct device *dev, struct
	device_attribute * attr, char *buf)
{
	struct ea8061 *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->acl_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t acl_control_store(struct device *dev, struct
	device_attribute * attr, const char *buf, size_t size)
{
	struct ea8061 *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (lcd->acl_enable != value) {
		dev_info(dev, "acl control changed from %d to %d\n",
						lcd->acl_enable, value);
		lcd->acl_enable = value;
		if (lcd->acl_enable)
			ea8061_acl_on(lcd);
		else
			ea8061_acl_off(lcd);
	}
	return size;
}

static ssize_t lcd_type_show(struct device *dev, struct
	device_attribute * attr, char *buf)
{
	struct ea8061 *lcd = dev_get_drvdata(dev);
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

static int ea8061_read_reg(struct ea8061 *lcd, unsigned int addr, char *buf)
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

static int ea8061_write_reg(struct ea8061 *lcd, char *name)
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
	struct ea8061 *lcd = dev_get_drvdata(dev);

	if (lcd->cur_addr == 0) {
		dev_err(dev, "failed to set current lcd register.\n");
		return -EINVAL;
	}

	return ea8061_read_reg(lcd, lcd->cur_addr, buf);
}

static ssize_t read_reg_store(struct device *dev, struct
	device_attribute * attr, const char *buf, size_t size)
{
	struct ea8061 *lcd = dev_get_drvdata(dev);
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
	struct ea8061 *lcd = dev_get_drvdata(dev);
	char name[32];
	int ret;

	ret = sscanf(buf, "%s", name);
	if (ret < 0)
		return ret;

	ret = ea8061_write_reg(lcd, name);
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

static struct device_attribute bd_device_attrs[] = {
	__ATTR(min_brightness, S_IRUGO,
			backlight_show_min_brightness, NULL),
};

static struct device_attribute ld_device_attrs[] = {
	__ATTR(acl_control, S_IRUGO|S_IWUSR|S_IWGRP,
			acl_control_show, acl_control_store),
	__ATTR(lcd_type, S_IRUGO,
			lcd_type_show, NULL),
	__ATTR(read_reg, S_IRUGO|S_IWUSR|S_IWGRP,
			read_reg_show, read_reg_store),
	__ATTR(write_reg, S_IWUSR|S_IWGRP,
			NULL, write_reg_store),
};

static struct panel_model ea8061_model[] = {
	{
		.ver = VER_161,			/* MACH_SLP_T0_LTE */
		.name = "SMD_AMS555HBxx-0",
	}
};

static void ea8061_regulator_ctl(struct ea8061 *lcd, bool enable)
{
	mutex_lock(&lcd->lock);

	if (enable) {
		if (lcd->reg_vdd3)
			regulator_enable(lcd->reg_vdd3);

		if (lcd->reg_vci)
			regulator_enable(lcd->reg_vci);
	} else {
		if (lcd->reg_vci)
			regulator_disable(lcd->reg_vci);

		if (lcd->reg_vdd3)
			regulator_disable(lcd->reg_vdd3);
	}

	mutex_unlock(&lcd->lock);
}

static void ea8061_power_on(struct mipi_dsim_lcd_device *dsim_dev,
				unsigned int enable)
{
	struct ea8061 *lcd = dev_get_drvdata(&dsim_dev->dev);

	dev_dbg(lcd->dev, "%s:enable[%d]\n", __func__, enable);

	if (enable) {
		/* lcd power on */
		ea8061_regulator_ctl(lcd, true);

		ea8061_delay(lcd->ddi_pd->reset_delay);

		/* lcd reset high */
		if (lcd->ddi_pd->reset)
			lcd->ddi_pd->reset(lcd->ld);

		/* wait more than 5ms */
		ea8061_delay(5);
	} else {
		/* lcd reset low */
		if (lcd->property->reset_low)
			lcd->property->reset_low();

		/* lcd power off */
		ea8061_regulator_ctl(lcd, false);
	}
}

static int ea8061_check_mtp(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct ea8061 *lcd = dev_get_drvdata(&dsim_dev->dev);
	u8 mtp_id[3] = {0, };

	ea8061_read_id(lcd, mtp_id);

	if (mtp_id[0] == 0x00) {
		dev_err(lcd->dev, "read id failed\n");
		return -EIO;
	}

	dev_info(lcd->dev,
		"Read ID : %x, %x, %x\n", mtp_id[0], mtp_id[1], mtp_id[2]);

	return 0;
}

static void ea8061_set_sequence(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct ea8061 *lcd = dev_get_drvdata(&dsim_dev->dev);

	ea8061_panel_init(lcd);
	ea8061_display_on(lcd);
}

static int ea8061_probe(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct ea8061 *lcd;
	int ret;
	int i;

	lcd = kzalloc(sizeof(struct ea8061), GFP_KERNEL);
	if (!lcd) {
		dev_err(&dsim_dev->dev, "failed to allocate ea8061 structure.\n");
		return -ENOMEM;
	}

	lcd->dsim_dev = dsim_dev;
	lcd->ddi_pd = (struct lcd_platform_data *)dsim_dev->platform_data;
	lcd->dev = &dsim_dev->dev;

	mutex_init(&lcd->lock);

	if (system_rev == 0x8)
		lcd->reg_vdd3 = regulator_get(lcd->dev, "VCC_LCD_1.8V");
	else
		lcd->reg_vdd3 = regulator_get(lcd->dev, "VDD3");
	if (IS_ERR(lcd->reg_vdd3)) {
		ret = PTR_ERR(lcd->reg_vdd3);
		dev_err(lcd->dev, "failed to get %s regulator (%d)\n",
				"VDD3", ret);
		lcd->reg_vdd3 = NULL;
	}

	lcd->reg_vci = regulator_get(lcd->dev, "VCI");
	if (IS_ERR(lcd->reg_vci)) {
		ret = PTR_ERR(lcd->reg_vci);
		dev_err(lcd->dev, "failed to get %s regulator (%d)\n",
				"VCI", ret);
		lcd->reg_vci = NULL;
	}

	lcd->ld = lcd_device_register("ea8061", lcd->dev, lcd,
			&ea8061_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		ret = PTR_ERR(lcd->ld);
		goto err_regulator;
	}

	lcd->bd = backlight_device_register("ea8061-bl", lcd->dev, lcd,
			&ea8061_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dev_err(lcd->dev, "failed to register backlight ops.\n");
		ret = PTR_ERR(lcd->bd);
		goto err_unregister_lcd;
	}

	ea8061_regulator_ctl(lcd, true);

	if (lcd->ddi_pd)
		lcd->property = lcd->ddi_pd->pdata;

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = MAX_BRIGHTNESS;
	lcd->power = FB_BLANK_UNBLANK;
	lcd->model = ea8061_model;
	lcd->model_count = ARRAY_SIZE(ea8061_model);

	for (i = 0; i < ARRAY_SIZE(bd_device_attrs); i++) {
		ret = device_create_file(&lcd->bd->dev,
					&bd_device_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "failed to add sysfs entries\n");
			break;
		}
	}

	for (i = 0; i < ARRAY_SIZE(ld_device_attrs); i++) {
		ret = device_create_file(&lcd->ld->dev,
					&ld_device_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "failed to add sysfs entries\n");
			break;
		}
	}

	dev_set_drvdata(&dsim_dev->dev, lcd);
	dev_info(lcd->dev, "probed ea8061 panel driver(%s).\n",
			dev_name(&lcd->ld->dev));

	return 0;

err_unregister_lcd:
	lcd_device_unregister(lcd->ld);

err_regulator:
	regulator_put(lcd->reg_vci);
	regulator_put(lcd->reg_vdd3);

	kfree(lcd);

	return ret;
}

static void ea8061_remove(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct ea8061 *lcd = dev_get_drvdata(&dsim_dev->dev);

	backlight_device_unregister(lcd->bd);
	lcd_device_unregister(lcd->ld);

	regulator_put(lcd->reg_vci);
	regulator_put(lcd->reg_vdd3);

	kfree(lcd);
}

#ifdef CONFIG_PM
static int ea8061_suspend(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct ea8061 *lcd = dev_get_drvdata(&dsim_dev->dev);

	ea8061_display_off(lcd);
	ea8061_sleep_in(lcd);
	ea8061_delay(lcd->ddi_pd->power_off_delay);

	return 0;
}

static int ea8061_resume(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct ea8061 *lcd = dev_get_drvdata(&dsim_dev->dev);

	ea8061_delay(lcd->ddi_pd->power_on_delay);

	return 0;
}
#else
#define ea8061_suspend		NULL
#define ea8061_resume		NULL
#endif

static struct mipi_dsim_lcd_driver ea8061_dsim_ddi_driver = {
	.name = "ea8061",
	.id = -1,

	.power_on = ea8061_power_on,
	.check_mtp = ea8061_check_mtp,
	.set_sequence = ea8061_set_sequence,
	.probe = ea8061_probe,
	.remove = ea8061_remove,
	.suspend = ea8061_suspend,
	.resume = ea8061_resume,
};

static int ea8061_init(void)
{
	s5p_mipi_dsi_register_lcd_driver(&ea8061_dsim_ddi_driver);

	return 0;
}

static void ea8061_exit(void)
{
	return;
}

module_init(ea8061_init);
module_exit(ea8061_exit);


MODULE_AUTHOR("Joongmock Shin <jmock.shin@samsung.com>");
MODULE_AUTHOR("Eunchul Kim <chulspro.kim@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based ea8061 AMOLED Panel Driver");
MODULE_LICENSE("GPL");

