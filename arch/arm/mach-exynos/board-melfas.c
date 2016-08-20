/*
 * linux/arch/arm/mach-exynos/board-melfas.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>
#include <mach/board-melfas.h>
#include <linux/pm_qos_params.h>

#ifdef CONFIG_CPU_FREQ_GOV_ONDEMAND_FLEXRATE
static void flexrate_work(struct work_struct *work)
{
	cpufreq_ondemand_flexrate_request(10000, 10);
}
static DECLARE_WORK(flex_work, flexrate_work);
#endif

static struct pm_qos_request_list busfreq_qos;
static void flexrate_qos_cancel(struct work_struct *work)
{
	pm_qos_update_request(&busfreq_qos, 0);
}

static DECLARE_DELAYED_WORK(busqos_work, flexrate_qos_cancel);

static void tsp_request_qos(void *data)
{
#ifdef CONFIG_CPU_FREQ_GOV_ONDEMAND_FLEXRATE
	if (!work_pending(&flex_work))
		schedule_work_on(0, &flex_work);
#endif
	/* Guarantee that the bus runs at >= 266MHz */
	if (!pm_qos_request_active(&busfreq_qos))
		pm_qos_add_request(&busfreq_qos, PM_QOS_BUS_DMA_THROUGHPUT,
				   266000);
	else {
		cancel_delayed_work_sync(&busqos_work);
		pm_qos_update_request(&busfreq_qos, 266000);
	}

	/* Cancel the QoS request after 1/10 sec */
	schedule_delayed_work_on(0, &busqos_work, HZ / 5);
}

#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS
/* MELFAS TSP */
#ifdef CONFIG_MACH_TRATS
static int melfas_mms_power(int on)
{
	if (on) {
		gpio_request(GPIO_TSP_LDO_ON, "TSP_LDO_ON");
		s3c_gpio_cfgpin(GPIO_TSP_LDO_ON, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_LDO_ON, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_LDO_ON, GPIO_LEVEL_HIGH);

		mdelay(70);
		gpio_request(GPIO_TSP_INT, "TSP_INT");
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));

		printk(KERN_INFO "[TSP]melfas power on\n");
		return 0;
	} else {
		gpio_request(GPIO_TSP_INT, "TSP_INT");
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);

		gpio_request(GPIO_TSP_LDO_ON, "TSP_LDO_ON");
		s3c_gpio_cfgpin(GPIO_TSP_LDO_ON, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_LDO_ON, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_TSP_LDO_ON, GPIO_LEVEL_LOW);

		printk(KERN_INFO "[TSP]melfas power on\n");
		return 0;
	}
}
#else
static bool enabled;
static int melfas_mms_power(int on)
{
	struct regulator *regulator_avdd;
	struct regulator *regulator_iovdd;

	if (enabled == on)
		return 0;

	/* Analog-Panel Power: 3.3V */
	regulator_avdd = regulator_get(NULL, "touch");
	if (IS_ERR(regulator_avdd))
		return PTR_ERR(regulator_avdd);

	/* IO Logic Power: 1.8V */
	regulator_iovdd = regulator_get(NULL, "touch_1.8v");
	if (IS_ERR(regulator_iovdd)) {
		regulator_put(regulator_avdd);
		return PTR_ERR(regulator_iovdd);
	}

	printk(KERN_DEBUG "[TSP] %s %s\n", __func__, on ? "on" : "off");

	if (on) {
		regulator_enable(regulator_avdd);
		regulator_enable(regulator_iovdd);
	} else {
		if (regulator_is_enabled(regulator_iovdd))
			regulator_disable(regulator_iovdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);
	}

	enabled = on;
	regulator_put(regulator_avdd);
	regulator_put(regulator_iovdd);

	return 0;
}
#endif
static int is_melfas_mms_vdd_on(void)
{
	int ret;
	/* 3.3V */
	static struct regulator *regulator;

	if (!regulator) {
		regulator = regulator_get(NULL, "touch");
		if (IS_ERR(regulator)) {
			ret = PTR_ERR(regulator);
			pr_err("could not get touch, rc = %d\n", ret);
			return ret;
		}
	}

	if (regulator_is_enabled(regulator))
		return 1;
	else
		return 0;
}

static int melfas_mms_mux_fw_flash(bool to_gpios)
{
	pr_info("%s:to_gpios=%d\n", __func__, to_gpios);

	/* TOUCH_EN is always an output */
	if (to_gpios) {
		if (gpio_request(GPIO_TSP_SCL_18V, "GPIO_TSP_SCL"))
			pr_err("failed to request gpio(GPIO_TSP_SCL)\n");
		if (gpio_request(GPIO_TSP_SDA_18V, "GPIO_TSP_SDA"))
			pr_err("failed to request gpio(GPIO_TSP_SDA)\n");

		gpio_direction_output(GPIO_TSP_INT, 0);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SCL_18V, 0);
		s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SDA_18V, 0);
		s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);

	} else {
		gpio_direction_output(GPIO_TSP_INT, 1);
		gpio_direction_input(GPIO_TSP_INT);
		s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
		/*s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT); */
		s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
		/*S3C_GPIO_PULL_UP */

		gpio_direction_output(GPIO_TSP_SCL_18V, 1);
		gpio_direction_input(GPIO_TSP_SCL_18V);
		s3c_gpio_cfgpin(GPIO_TSP_SCL_18V, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(GPIO_TSP_SCL_18V, S3C_GPIO_PULL_NONE);

		gpio_direction_output(GPIO_TSP_SDA_18V, 1);
		gpio_direction_input(GPIO_TSP_SDA_18V);
		s3c_gpio_cfgpin(GPIO_TSP_SDA_18V, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(GPIO_TSP_SDA_18V, S3C_GPIO_PULL_NONE);

		gpio_free(GPIO_TSP_SCL_18V);
		gpio_free(GPIO_TSP_SDA_18V);
	}
	return 0;
}

static struct tsp_callbacks *charger_callbacks;
static struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *, bool);
};

void tsp_charger_infom(bool en)
{
	if (charger_callbacks && charger_callbacks->inform_charger)
		charger_callbacks->inform_charger(charger_callbacks, en);
}

static void melfas_register_callback(void *cb)
{
	charger_callbacks = cb;
	pr_debug("[TSP] melfas_register_callback\n");
}

static struct melfas_mms_platform_data mms_ts_pdata = {
	.max_x = 720,
	.max_y = 1280,
	.invert_x = 0,
	.invert_y = 0,
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL_18V,
	.gpio_sda = GPIO_TSP_SDA_18V,
	.auto_update = false,
	.power = melfas_mms_power,
	.mux_fw_flash = melfas_mms_mux_fw_flash,
	.is_vdd_on = is_melfas_mms_vdd_on,
	.input_event = tsp_request_qos,
	.register_cb = melfas_register_callback,
};

static struct melfas_mms_platform_data mms_ts_pdata_rotate = {
	.max_x = 720,
	.max_y = 1280,
	.invert_x = 720,
	.invert_y = 1280,
	.gpio_int = GPIO_TSP_INT,
	.gpio_scl = GPIO_TSP_SCL_18V,
	.gpio_sda = GPIO_TSP_SDA_18V,
	.auto_update = false,
	.power = melfas_mms_power,
	.mux_fw_flash = melfas_mms_mux_fw_flash,
	.is_vdd_on = is_melfas_mms_vdd_on,
	.input_event = tsp_request_qos,
	.register_cb = melfas_register_callback,
};

static struct i2c_board_info i2c_devs3[] = {
	{
	 I2C_BOARD_INFO(MELFAS_TS_NAME, 0x48),
	 .platform_data = &mms_ts_pdata},
};

void __init melfas_tsp_set_platdata(bool rotate)
{
	if (rotate)
		i2c_devs3[0].platform_data = &mms_ts_pdata_rotate;
	else
		i2c_devs3[0].platform_data = &mms_ts_pdata;

}

void __init melfas_tsp_set_pdata(bool rotate, bool force)
{
	if (rotate) {
		mms_ts_pdata_rotate.auto_update = force;
		i2c_devs3[0].platform_data = &mms_ts_pdata_rotate;
	} else {
		i2c_devs3[0].platform_data = &mms_ts_pdata;
		mms_ts_pdata.auto_update = force;
	}
}

#ifdef CONFIG_MACH_TRATS
void __init melfas_tsp_init(void)
{
	int gpio;

	/* TSP_LDO_ON: XMDMADDR_11 */
	gpio = GPIO_TSP_LDO_ON;
	gpio_request(gpio, "TSP_LDO_ON");
	gpio_direction_output(gpio, 1);
	gpio_export(gpio, 0);

	/* TSP_INT: XMDMADDR_7 */
	gpio = GPIO_TSP_INT;
	gpio_request(gpio, "TSP_INT");
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	i2c_devs3[0].irq = gpio_to_irq(gpio);

	printk(KERN_INFO "%s touch : %d\n", __func__, i2c_devs3[0].irq);

	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
}
#else
void __init melfas_tsp_init(void)
{
	int gpio;
	int ret;
	printk(KERN_INFO "[TSP] melfas_tsp_init() is called\n");

	gpio = GPIO_TSP_INT;
	ret = gpio_request(gpio, "TSP_INT");
	if (ret)
		pr_err("failed to request gpio(TSP_INT)\n");
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);

	s5p_register_gpio_interrupt(gpio);
	i2c_devs3[0].irq = gpio_to_irq(gpio);

	printk(KERN_INFO "%s touch : %d\n", __func__, i2c_devs3[0].irq);

	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
}
#endif
#endif

