/*
 *  Haptic Class Core
 *
 *  Copyright (C) 2008 Samsung Electronics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/rwsem.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/haptic.h>
#include "haptic.h"

static DECLARE_RWSEM(haptic_list_lock);
static LIST_HEAD(haptic_list);
static struct class *haptic_class;

static void haptic_update_value(struct haptic_classdev *haptic_cdev)
{
	if (haptic_cdev->get)
		haptic_cdev->value = haptic_cdev->get(haptic_cdev);
}

static ssize_t haptic_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev);
	ssize_t ret = 0;

	/* no lock needed for this */
	haptic_update_value(haptic_cdev);
	sprintf(buf, "%u\n", haptic_get_value(haptic_cdev));
	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t haptic_value_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct haptic_classdev *haptic_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;
	unsigned long val;

	ret = strict_strtoul(buf, 10, &val);
	if (ret == 0) {
		ret = size;
		haptic_set_value(haptic_cdev, val);
	}

	return ret;
}

static DEVICE_ATTR(value, 0644, haptic_value_show, haptic_value_store);

/**
 * haptic_classdev_suspend - suspend an haptic_classdev.
 * @haptic_cdev: the haptic_classdev to suspend.
 */
void haptic_classdev_suspend(struct haptic_classdev *haptic_cdev)
{
	haptic_cdev->flags |= HAPTIC_SUSPENDED;
	haptic_cdev->set(haptic_cdev, HAPTIC_OFF);
}
EXPORT_SYMBOL_GPL(haptic_classdev_suspend);

/**
 * haptic_classdev_resume - resume an haptic_classdev.
 * @haptic_cdev: the haptic_classdev to resume.
 */
void haptic_classdev_resume(struct haptic_classdev *haptic_cdev)
{
	haptic_cdev->set(haptic_cdev, haptic_cdev->value);
	haptic_cdev->flags &= ~HAPTIC_SUSPENDED;
}
EXPORT_SYMBOL_GPL(haptic_classdev_resume);

/**
 * haptic_classdev_register - register a new object of haptic_classdev class.
 * @dev: The device to register.
 * @haptic_cdev: the haptic_classdev structure for this device.
 */
int haptic_classdev_register(struct device *parent,
				struct haptic_classdev *haptic_cdev)
{
	int ret;

	haptic_cdev->dev = device_create(haptic_class, parent, 0,
				haptic_cdev, "%s", haptic_cdev->name);
	if (IS_ERR(haptic_cdev->dev))
		return PTR_ERR(haptic_cdev->dev);

	/* register the attributes */
	ret = device_create_file(haptic_cdev->dev, &dev_attr_value);
	if (ret)
		goto err_out;

	/* add to the list of haptic */
	down_write(&haptic_list_lock);
	list_add_tail(&haptic_cdev->node, &haptic_list);
	up_write(&haptic_list_lock);

	haptic_update_value(haptic_cdev);

	printk(KERN_INFO "Registered haptic device: %s\n", haptic_cdev->name);
	return 0;

err_out:
	device_unregister(haptic_cdev->dev);
	return ret;
}
EXPORT_SYMBOL_GPL(haptic_classdev_register);

/**
 * haptic_classdev_unregister - unregisters a object of haptic_properties class.
 * @haptic_cdev: the haptic device to unregister
 *
 * Unregisters a previously registered via haptic_classdev_register object.
 */
void haptic_classdev_unregister(struct haptic_classdev *haptic_cdev)
{
	device_remove_file(haptic_cdev->dev, &dev_attr_value);
	device_unregister(haptic_cdev->dev);

	down_write(&haptic_list_lock);
	list_del(&haptic_cdev->node);
	up_write(&haptic_list_lock);
}
EXPORT_SYMBOL_GPL(haptic_classdev_unregister);

static int __init haptic_init(void)
{
	haptic_class = class_create(THIS_MODULE, "haptic");
	if (IS_ERR(haptic_class))
		return PTR_ERR(haptic_class);
	return 0;
}
subsys_initcall(haptic_init);

static void __exit haptic_exit(void)
{
	class_destroy(haptic_class);
}
module_exit(haptic_exit);

MODULE_AUTHOR("Kyungmin Park <kyungmin.park@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Haptic Class Interface");
