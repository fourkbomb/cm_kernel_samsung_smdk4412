/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include "drmP.h"

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/module.h>

#include "exynos_drm_drv.h"
#include "exynos_hdmi.h"


static int exynos_hdmi_phy_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	exynos_hdmi_attach_hdmiphy_client(client);

	dev_info(&client->adapter->dev, "attached s5p_hdmiphy "
		"into i2c adapter successfully\n");

	return 0;
}

static int exynos_hdmi_phy_remove(struct i2c_client *client)
{
	dev_info(&client->adapter->dev, "detached s5p_hdmiphy "
		"from i2c adapter successfully\n");

	return 0;
}

static const struct i2c_device_id hdmi_phy_idtable[] = {
	{ "s5p_hdmiphy", 0 },
	{ },
};

struct i2c_driver hdmi_phy_driver = {
	.driver = {
		.name	= "s5p-hdmiphy",
		.owner	= THIS_MODULE,
	},
	.id_table = hdmi_phy_idtable,
	.probe		= exynos_hdmi_phy_probe,
	.remove		= __devexit_p(exynos_hdmi_phy_remove),
	.command		= NULL,
};
