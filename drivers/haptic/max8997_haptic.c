/*
 *  max8997_haptic.c - Haptic controller driver
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/haptic.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/ctype.h>
#include <linux/workqueue.h>
#include <linux/mfd/max8997-private.h>
#include <linux/mfd/max8997.h>
#include "haptic.h"

#define PWM_ERROR			1
#define PWM_HAPTIC_PERIOD		(38022 * 2)
#define PWM_HAPTIC_DEFAULT_LEVEL	99

#define MAX8997_ERM_MODE		(0 << 7)
#define MAX8997_LRA_MODE		(1 << 7)
#define MAX8997_ENABLE			(1 << 6)
#define MAX8997_DISABLE			(0 << 6)
#define MAX8997_EXTERNAL_PWM		(0 << 5)
#define MAX8997_INTERNAL_PWM		(1 << 5)
#define MAX8997_PWM_DIVISOR_32		(0)
#define MAX8997_PWM_DIVISOR_64		(1)
#define MAX8997_PWM_DIVISOR_128		(2)
#define MAX8997_PWM_DIVISOR_256		(3)

static DEFINE_MUTEX(vib_lock);

struct max8997_haptic {
	struct device *dev;
	struct i2c_client *client;
	int pwm_timer;
	struct haptic_classdev cdev;
	struct work_struct work;
	struct timer_list timer;
	void (*control_power)(struct device *dev, int);

	int enable;
	int powered;

	int level;
	int level_max;
};

static inline struct max8997_haptic *cdev_to_max8997_haptic(
		struct haptic_classdev *haptic_cdev)
{
	return container_of(haptic_cdev, struct max8997_haptic, cdev);
}

static int max8997_haptic_set_pwm_cycle(struct max8997_haptic *haptic)
{
	int duty = PWM_HAPTIC_PERIOD * (haptic->level / 2 + 50) / 100;
	struct pwm_device *pwm = pwm_request(haptic->pwm_timer, "haptic");
	int ret;

	if (IS_ERR(pwm)) {
		dev_err(haptic->cdev.dev,
				"unable to request PWM for haptic.\n");
		return PWM_ERROR;
	}
	ret = pwm_config(pwm, duty, PWM_HAPTIC_PERIOD);
	pwm_free(pwm);
	return ret;
}

static int max8997_haptic_write_reg(struct i2c_client *client,
						int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static void max8997_haptic_setup(struct i2c_client *client);

static void max8997_haptic_power_on(struct max8997_haptic *haptic)
{
	struct i2c_client *client = haptic->client;
	struct pwm_device *pwm;

	if (haptic->powered)
		return;
	haptic->powered = 1;

	if (haptic->control_power) {
		mutex_lock(&vib_lock);
		haptic->control_power(haptic->dev, 1);
		mutex_unlock(&vib_lock);
	}

	max8997_haptic_setup(client);

	pwm = pwm_request(haptic->pwm_timer, "haptic");
	if (IS_ERR(pwm)) {
		dev_err(haptic->cdev.dev, "unable to request PWM for haptic.\n");
		return;
	}
	pwm_enable(pwm);
	pwm_free(pwm);
}

static void max8997_haptic_power_off(struct max8997_haptic *haptic)
{
	struct i2c_client *client = haptic->client;
	struct pwm_device *pwm;
	int value;

	value = MAX8997_LRA_MODE | MAX8997_DISABLE | MAX8997_EXTERNAL_PWM |
		MAX8997_PWM_DIVISOR_128;
	max8997_haptic_write_reg(client, MAX8997_HAPTIC_REG_CONF2, value);

	if (!haptic->powered)
		return;
	haptic->powered = 0;

	pwm = pwm_request(haptic->pwm_timer, "haptic");
	if (IS_ERR(pwm)) {
		dev_err(haptic->cdev.dev, "unable to request PWM for haptic.\n");
		return;
	}
	pwm_disable(pwm);
	pwm_free(pwm);

	if (haptic->control_power) {
		mutex_lock(&vib_lock);
		haptic->control_power(haptic->dev, 0);
		mutex_unlock(&vib_lock);
	}
}

static void max8997_haptic_work(struct work_struct *work)
{
	struct max8997_haptic *haptic;
	int r;

	haptic = container_of(work, struct max8997_haptic, work);
	if (haptic->enable) {
		r = max8997_haptic_set_pwm_cycle(haptic);
		if (r) {
			dev_dbg(haptic->cdev.dev, "set_pwm_cycle failed\n");
			return;
		}
		max8997_haptic_power_on(haptic);
	} else {
		max8997_haptic_power_off(haptic);
	}
}

static void max8997_haptic_timer(unsigned long data)
{
	struct max8997_haptic *haptic = (struct max8997_haptic *)data;

	haptic->enable = 0;
	schedule_work(&haptic->work);
}

static void max8997_haptic_set(struct haptic_classdev *haptic_cdev,
				enum haptic_value value)
{
	struct max8997_haptic *haptic =
		cdev_to_max8997_haptic(haptic_cdev);

	switch (value) {
	case HAPTIC_OFF:
		haptic->enable = 0;
		break;
	case HAPTIC_HALF:
	case HAPTIC_FULL:
	default:
		haptic->enable = 1;
		break;
	}

	schedule_work(&haptic->work);
}

static enum haptic_value max8997_haptic_get(struct haptic_classdev *haptic_cdev)
{
	struct max8997_haptic *haptic =
		cdev_to_max8997_haptic(haptic_cdev);

	if (haptic->enable)
		return HAPTIC_FULL;

	return HAPTIC_OFF;
}

#define ATTR_DEF_SHOW(name) \
static ssize_t max8997_haptic_show_##name(struct device *dev, \
		struct device_attribute *attr, char *buf) \
{ \
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev); \
	struct max8997_haptic *haptic = cdev_to_max8997_haptic(haptic_cdev); \
\
	return sprintf(buf, "%u\n", haptic->name) + 1; \
}

#define ATTR_DEF_STORE(name) \
static ssize_t max8997_haptic_store_##name(struct device *dev, \
		struct device_attribute *attr, \
		const char *buf, size_t size) \
{ \
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev); \
	struct max8997_haptic *haptic = cdev_to_max8997_haptic(haptic_cdev); \
	ssize_t ret = -EINVAL; \
	unsigned long val; \
\
	ret = strict_strtoul(buf, 10, &val); \
	if (ret == 0) { \
		ret = size; \
		haptic->name = val; \
		schedule_work(&haptic->work); \
	} \
\
	return ret; \
}

ATTR_DEF_SHOW(enable);
ATTR_DEF_STORE(enable);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, max8997_haptic_show_enable,
		max8997_haptic_store_enable);

static ssize_t max8997_haptic_store_level(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev);
	struct max8997_haptic *haptic = cdev_to_max8997_haptic(haptic_cdev);
	ssize_t ret = -EINVAL;
	unsigned long val;

	ret = strict_strtoul(buf, 10, &val);
	if (ret == 0) {
		ret = size;
		if (haptic->level_max < val)
			val = haptic->level_max;
		haptic->level = val;
		schedule_work(&haptic->work);
	}

	return ret;
}
ATTR_DEF_SHOW(level);
static DEVICE_ATTR(level, S_IRUGO | S_IWUSR, max8997_haptic_show_level,
		max8997_haptic_store_level);

ATTR_DEF_SHOW(level_max);
static DEVICE_ATTR(level_max, S_IRUGO, max8997_haptic_show_level_max, NULL);

static ssize_t max8997_haptic_store_oneshot(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev);
	struct max8997_haptic *haptic = cdev_to_max8997_haptic(haptic_cdev);
	ssize_t ret = -EINVAL;
	unsigned long val;

	ret = strict_strtoul(buf, 10, &val);
	if (ret == 0) {
		ret = size;
		haptic->enable = 1;
		mod_timer(&haptic->timer, jiffies + val * HZ / 1000);
		schedule_work(&haptic->work);
	}

	return ret;
}
static DEVICE_ATTR(oneshot, S_IWUSR, NULL, max8997_haptic_store_oneshot);

static struct attribute *haptic_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_level.attr,
	&dev_attr_level_max.attr,
	&dev_attr_oneshot.attr,
	NULL,
};

static const struct attribute_group haptic_group = {
	.attrs = haptic_attributes,
};

static void max8997_haptic_setup(struct i2c_client *client)
{
	int value;

	value = MAX8997_LRA_MODE | MAX8997_ENABLE | MAX8997_EXTERNAL_PWM |
		MAX8997_PWM_DIVISOR_128;
	max8997_haptic_write_reg(client, MAX8997_HAPTIC_REG_CONF2, value);
}

static int __devinit max8997_haptic_probe(struct platform_device *pdev)
{
	struct max8997_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max8997_platform_data *mpdata = dev_get_platdata(iodev->dev);
	struct max8997_haptic_platform_data *pdata = mpdata->haptic_pdata;
	struct max8997_haptic *chip;
	int ret;

	if (!pdata) {
		dev_err(&pdev->dev, "%s: no platform data\n", __func__);
		return -EINVAL;
	}
	chip = kzalloc(sizeof(struct max8997_haptic), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = iodev->hmotor;
	chip->cdev.set = max8997_haptic_set;
	chip->cdev.get = max8997_haptic_get;
	chip->cdev.name = pdata->name;
	chip->enable = 0;
	chip->level = PWM_HAPTIC_DEFAULT_LEVEL;
	chip->level_max = PWM_HAPTIC_DEFAULT_LEVEL;
	chip->pwm_timer = pdata->pwm_timer;
	chip->dev = &pdev->dev;

	if (pdata->control_power)
		chip->control_power = pdata->control_power;

	INIT_WORK(&chip->work, max8997_haptic_work);

	/* register our new haptic device */
	ret = haptic_classdev_register(&pdev->dev, &chip->cdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "haptic_classdev_register failed\n");
		goto error_classdev;
	}

	ret = sysfs_create_group(&chip->cdev.dev->kobj, &haptic_group);
	if (ret)
		goto error_create_group;

	init_timer(&chip->timer);
	chip->timer.data = (unsigned long)chip;
	chip->timer.function = &max8997_haptic_timer;

	platform_set_drvdata(pdev, chip);

	max8997_haptic_power_off(chip);

	dev_info(&pdev->dev, "max8997_haptic %s registered\n", pdev->name);
	return 0;

error_create_group:
	haptic_classdev_unregister(&chip->cdev);
error_classdev:
	kfree(chip);
	return ret;
}

static int __devexit max8997_haptic_remove(struct platform_device *pdev)
{
	struct max8997_haptic *chip = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &haptic_group);
	haptic_classdev_unregister(&chip->cdev);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_PM
static int max8997_haptic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct max8997_haptic *chip = platform_get_drvdata(pdev);

	max8997_haptic_power_off(chip);

	return 0;
}

static int max8997_haptic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct max8997_haptic *chip = platform_get_drvdata(pdev);

	max8997_haptic_setup(chip->client);

	return 0;
}

static const struct dev_pm_ops max8997_dev_pm_ops = {
	.suspend		= max8997_haptic_suspend,
	.resume			= max8997_haptic_resume,
};

#define MAX8997_DEV_PM_OPS	(&max8997_dev_pm_ops)
#else
#define MAX8997_DEV_PM_OPS	NULL
#endif

static const struct platform_device_id max8997_haptic_id[] = {
	{ "max8997-hapticmotor", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max8997_haptic_id);

static struct platform_driver max8997_haptic_driver = {
	.driver	= {
		.name	= "max8997-hapticmotor",
		.owner	= THIS_MODULE,
		.pm	= MAX8997_DEV_PM_OPS,
	},
	.probe		= max8997_haptic_probe,
	.remove		= __devexit_p(max8997_haptic_remove),
	.id_table	= max8997_haptic_id,
};

static int __init max8997_haptic_init(void)
{
	return platform_driver_register(&max8997_haptic_driver);
}

static void __exit max8997_haptic_exit(void)
{
	platform_driver_unregister(&max8997_haptic_driver);
}

module_init(max8997_haptic_init);
module_exit(max8997_haptic_exit);

MODULE_AUTHOR("Donggeun Kim <dg77.kim@samsung.com>");
MODULE_DESCRIPTION("max8997_haptic motor driver");
MODULE_LICENSE("GPL");
