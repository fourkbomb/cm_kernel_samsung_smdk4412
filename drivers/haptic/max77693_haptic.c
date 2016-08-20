/*
 * max77693_haptic.c - Haptic controller driver
 *
 * Copyright (C) 2012 Samsung Electronics
 * Kwang-Hui Cho <kwanghui.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/haptic.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/ctype.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max77693-private.h>
#include <linux/mfd/max77693.h>
#include "haptic.h"

#define PWM_HAPTIC_DEFAULT_LEVEL        99

#define MOTOR_LRA	(1<<7)
#define MOTOR_EN	(1<<6)
#define EXT_PWM		(0<<5)
#define DIVIDER_128	(1<<1)
#define DIVIDER_256	(1<<0 | 1<<1)

struct max77693_haptic {
	struct device *dev;
	struct i2c_client *i2c;
	struct i2c_client *pmic_i2c;
	struct haptic_classdev cdev;
	struct work_struct work;
	struct timer_list timer;
	struct pwm_device *pwm_dev;
	struct regulator *regulator;

	int enable;
	int powered;

	int level;
	int level_max;

	int pwm_duty;
	int pwm_period;
};

static inline struct max77693_haptic *cdev_to_max77693_haptic(
	struct haptic_classdev *haptic_cdev)
{
	return container_of(haptic_cdev, struct max77693_haptic, cdev);
}

static void max77693_haptic_setup(struct i2c_client *pmic_i2c,
						struct i2c_client *i2c)
{
	int ret;
	int value;

	value = MOTOR_LRA | EXT_PWM | DIVIDER_128 | MOTOR_EN;

	ret = max77693_update_reg(pmic_i2c,
				MAX77693_PMIC_REG_LSCNFG, 0x80, 0x80);
	if (ret)
		dev_err(&pmic_i2c->dev, "PMIC i2c update error 0x%x\n", ret);

	ret = max77693_write_reg(i2c, MAX77693_HAPTIC_REG_CONFIG2, value);
	if (ret)
		dev_err(&i2c->dev, "i2c write error 0x%x\n", ret);
}

static void max77693_haptic_power_on(struct max77693_haptic *haptic)
{
	struct i2c_client *i2c = haptic->i2c;
	struct i2c_client *pmic_i2c = haptic->pmic_i2c;

	if (haptic->powered)
		return;
	haptic->powered = 1;

	regulator_enable(haptic->regulator);

	max77693_haptic_setup(pmic_i2c, i2c);

	pwm_enable(haptic->pwm_dev);
}

static void max77693_haptic_power_off(struct max77693_haptic *haptic)
{
	struct i2c_client *i2c = haptic->i2c;
	struct i2c_client *pmic_i2c = haptic->pmic_i2c;
	int ret;
	int value;

	value = MOTOR_LRA | EXT_PWM | DIVIDER_128;

	ret = max77693_update_reg(pmic_i2c,
				MAX77693_PMIC_REG_LSCNFG, 0x00, 0x80);
	if (ret)
		dev_err(&pmic_i2c->dev, "PMIC i2c update error 0x%x\n", ret);

	ret = max77693_write_reg(i2c, MAX77693_HAPTIC_REG_CONFIG2, value);
	if (ret)
		dev_err(&i2c->dev, "i2c write error 0x%x\n", ret);

	if (!haptic->powered)
		return;
	haptic->powered = 0;

	pwm_disable(haptic->pwm_dev);
	regulator_disable(haptic->regulator);

}

static void max77693_haptic_set_pwm_cycle(struct max77693_haptic *haptic)
{
	int min = haptic->pwm_period / 2;
	int delta = (haptic->pwm_period * haptic->level) / 200;

	haptic->pwm_duty = min + delta;

	pwm_config(haptic->pwm_dev, haptic->pwm_duty, haptic->pwm_period);
}

static void max77693_haptic_work(struct work_struct *work)
{
	struct max77693_haptic *haptic;

	haptic = container_of(work, struct max77693_haptic, work);

	if (haptic->enable) {
		max77693_haptic_set_pwm_cycle(haptic);
		max77693_haptic_power_on(haptic);
	} else {
		max77693_haptic_power_off(haptic);
	}
}

static void max77693_haptic_timer(unsigned long data)
{
	struct max77693_haptic *haptic = (struct max77693_haptic *)data;

	haptic->enable = 0;
	schedule_work(&haptic->work);
}

static void max77693_haptic_set(struct haptic_classdev *haptic_cdev,
					enum haptic_value value)
{
	struct max77693_haptic *haptic =
				cdev_to_max77693_haptic(haptic_cdev);

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

static enum haptic_value max77693_haptic_get(
					struct haptic_classdev *haptic_cdev)
{
	struct max77693_haptic *haptic =
				cdev_to_max77693_haptic(haptic_cdev);

	if (haptic->enable)
		return HAPTIC_FULL;

	return HAPTIC_OFF;
}
/* show attribute template */
#define ATTR_DEF_SHOW(name) \
static ssize_t max77693_haptic_show_##name(struct device *dev, \
			struct device_attribute *attr, char *buf) \
{ \
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev); \
	struct max77693_haptic *haptic = cdev_to_max77693_haptic(haptic_cdev); \
\
	return sprintf(buf, "%u\n", haptic->name) + 1; \
}

/* enable attribute */
ATTR_DEF_SHOW(enable);
static ssize_t max77693_haptic_store_enable(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev);
	struct max77693_haptic *haptic = cdev_to_max77693_haptic(haptic_cdev);
	ssize_t ret = -EINVAL;
	unsigned long val;

	ret = strict_strtoul(buf, 10, &val);
	if (ret == 0) {
		ret = size;
		haptic->enable = val;
		schedule_work(&haptic->work);
	}
	return ret;
}
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, max77693_haptic_show_enable,
		max77693_haptic_store_enable);

/* level attribute */
ATTR_DEF_SHOW(level);
static ssize_t max77693_haptic_store_level(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev);
	struct max77693_haptic *haptic = cdev_to_max77693_haptic(haptic_cdev);
	ssize_t ret = -EINVAL;
	unsigned long val;

	ret = strict_strtoul(buf, 10, &val);
	if (ret == 0) {
		ret = size;
		if (val > haptic->level_max)
			val = haptic->level_max;
		haptic->level = val;
		schedule_work(&haptic->work);
	}
	return ret;
}
static DEVICE_ATTR(level, S_IRUGO | S_IWUSR, max77693_haptic_show_level,
		max77693_haptic_store_level);

/* level-max attribute */
ATTR_DEF_SHOW(level_max);
static DEVICE_ATTR(level_max, S_IRUGO, max77693_haptic_show_level_max, NULL);

/* oneshot attribute */
static ssize_t max77693_haptic_store_oneshot(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev);
	struct max77693_haptic *haptic = cdev_to_max77693_haptic(haptic_cdev);
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
static DEVICE_ATTR(oneshot, S_IWUSR, NULL, max77693_haptic_store_oneshot);

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

/* platform driver */

static int __devinit max77693_haptic_probe(struct platform_device *pdev)
{
	struct max77693_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77693_platform_data *max77693_pdata
					= dev_get_platdata(iodev->dev);
	struct max77693_haptic_platform_data *pdata
					= max77693_pdata->haptic_data;
	struct max77693_haptic *chip;
	int ret;

	if (!pdata) {
		dev_err(&pdev->dev, "%s: no platform data\n", __func__);
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct max77693_haptic), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->i2c = iodev->haptic;
	chip->pmic_i2c = iodev->i2c;
	chip->cdev.set = max77693_haptic_set;
	chip->cdev.get = max77693_haptic_get;
	chip->cdev.name = pdata->name;
	chip->enable = 0;
	chip->level = PWM_HAPTIC_DEFAULT_LEVEL;
	chip->level_max = PWM_HAPTIC_DEFAULT_LEVEL;
	chip->dev = &pdev->dev;
	chip->pwm_dev = pwm_request(pdata->pwm_id, "haptic");
	if (IS_ERR(chip->pwm_dev)) {
		pr_err("haptic pwm_request failed\n");
		ret = -EFAULT;
		goto error_pwm_request;
	}
	chip->pwm_duty = pdata->pwm_duty;
	chip->pwm_period = pdata->pwm_period;
	pwm_config(chip->pwm_dev, chip->pwm_duty, chip->pwm_period);

	chip->regulator = regulator_get(NULL, pdata->regulator_name);
	if (IS_ERR(chip->regulator)) {
		pr_err("get regulator %s failed\n", pdata->regulator_name);
		ret = -EFAULT;
		goto error_regulator;
	}

	INIT_WORK(&chip->work, max77693_haptic_work);

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
	chip->timer.function = &max77693_haptic_timer;

	platform_set_drvdata(pdev, chip);

	max77693_haptic_power_off(chip);

	dev_info(&pdev->dev, "max77693_haptic %s registered\n", pdev->name);
	return 0;

error_create_group:
	haptic_classdev_unregister(&chip->cdev);
error_classdev:
	regulator_put(chip->regulator);
error_regulator:
	pwm_free(chip->pwm_dev);
error_pwm_request:
	kfree(chip);
	return ret;
}

static int __devexit max77693_haptic_remove(struct platform_device *pdev)
{
	struct max77693_haptic *chip = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &haptic_group);
	haptic_classdev_unregister(&chip->cdev);
	kfree(chip);

	return 0;
}

#ifdef CONFIG_PM
static int max77693_haptic_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct max77693_haptic *chip = platform_get_drvdata(pdev);

	max77693_haptic_power_off(chip);

	return 0;
}

static int max77693_haptic_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct max77693_haptic *chip = platform_get_drvdata(pdev);

	max77693_haptic_setup(chip->pmic_i2c, chip->i2c);

	return 0;
}
#endif

static const struct dev_pm_ops max77693_dev_pm_ops = {
	.suspend = max77693_haptic_suspend,
	.resume = max77693_haptic_resume,
};

static const struct platform_device_id max77693_haptic_id[] = {
	{ "max77693-haptic", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, max77693_haptic_id);

static struct platform_driver max77693_haptic_driver = {
	.driver = {
		.name = "max77693-haptic",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &max77693_dev_pm_ops,
#endif
	},
	.probe = max77693_haptic_probe,
	.remove = __devexit_p(max77693_haptic_remove),
	.id_table = max77693_haptic_id,
};

static int __init max77693_haptic_init(void)
{
	return platform_driver_register(&max77693_haptic_driver);
}

static void __exit max77693_haptic_exit(void)
{
	platform_driver_unregister(&max77693_haptic_driver);
}
module_init(max77693_haptic_init);
module_exit(max77693_haptic_exit);

MODULE_AUTHOR("Kwang-Hui Cho <kwanghui.cho@samsung.com>");
MODULE_DESCRIPTION("max77693_haptic motor driver");
MODULE_LICENSE("GPL");
