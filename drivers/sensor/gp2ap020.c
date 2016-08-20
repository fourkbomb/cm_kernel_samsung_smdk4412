/*
 * Copyright (c) 2010 SAMSUNG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <mach/hardware.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/sensor/gp2ap020.h>
#include <linux/sensor/sensors_core.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>

/* for debugging */
#undef DEBUG

#define GP2A_VENDOR	"SHARP"
#define GP2A_CHIP_ID		"GP2AP"

#define PROX_READ_NUM	10

#define SENSOR_NAME "light_sensor"

#ifdef CONFIG_SLP
#define OFFSET_FILE_PATH	"/csa/sensor/prox_cal_data"
#endif
#define OFFSET_AVERAGE_LENGTH		10
#define ONOFF_OFFSET				2
#define DEFAULT_ON_THRESHOLD		8

#define SENSOR_DEFAULT_DELAY            (200)	/* 200 ms */
#define SENSOR_MAX_DELAY                (2000)	/* 2000 ms */

static struct i2c_driver opt_i2c_driver;
static struct i2c_client *opt_i2c_client;

struct opt_state {
	struct i2c_client *client;
};

struct gp2a_data {
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct work_struct proximity_work;	/* for proximity sensor */
	struct mutex light_mutex;
	struct mutex data_mutex;
	struct delayed_work light_work;
	struct device *proximity_dev;
	struct device *light_dev;
	struct gp2a_platform_data *pdata;
	struct wake_lock prx_wake_lock;

	int proximity_enabled;
	int proximity_average_on;
	int light_enabled;
	u8 lightsensor_mode;		/* 0 = low, 1 = high */
	bool light_data_first;
	int prox_data;
	int irq;
	int average[PROX_READ_NUM];	/*for proximity adc average */
	int light_delay;
	char proximity_detection;
	int offset_value;
	int orig_on_threshold;
};

/* initial value for sensor register */
#define COL 8
static u8 gp2a_original_image[COL][2] = {
	/*  {Regster, Value} */
	/*PRST :01(4 cycle at Detection/Non-detection),
	   ALSresolution :16bit, range *128   //0x1F -> 5F by sharp */
	{0x01, 0x63},
	/*ALC : 0, INTTYPE : 1, PS mode resolution : 12bit, range*1 */
	{0x02, 0x72},
	/*LED drive current 110mA, Detection/Non-detection judgment output */
	{0x03, 0x3C},
	/*	{0x04 , 0x00}, */
	/*	{0x05 , 0x00}, */
	/*	{0x06 , 0xFF}, */
	/*	{0x07 , 0xFF}, */
	{0x08, 0x07},		/*PS mode LTH(Loff):  (??mm) */
	{0x09, 0x00},		/*PS mode LTH(Loff) : */
	{0x0A, 0x08},		/*PS mode HTH(Lon) : (??mm) */
	{0x0B, 0x00},		/* PS mode HTH(Lon) : */
	/* {0x13 , 0x08}, by sharp for internal calculation (type:0) */
	/*alternating mode (PS+ALS), TYPE=1
	   (0:externel 1:auto calculated mode) //umfa.cal */
	{0x00, 0xC0}
};

static int proximity_onoff(u8 onoff, struct gp2a_data *data);
static int lightsensor_get_adc(struct gp2a_data *data);
static int lightsensor_onoff(u8 onoff, struct gp2a_data *data);
static int lightsensor_get_adcvalue(struct gp2a_data *data);
static int opt_i2c_init(void);

static int is_gp2a030a(struct gp2a_data *data)
{
	if (data->pdata->gp2a_check_version)
		return data->pdata->gp2a_check_version();

	return 0;
}

static int gp2a_update_threshold(u8 (*selected_image)[2],
				unsigned long new_threshold, bool update_reg)
{
	int i, err = 0;
	u8 set_value;

	for (i = 0; i < COL; i++) {
		switch (selected_image[i][0]) {
		case PS_LT_LSB:
			/*PS mode LTH(Loff) for low 8bit*/
			set_value = (new_threshold-ONOFF_OFFSET) & 0x00FF;
			break;

		case PS_LT_MSB:
			/*PS mode LTH(Loff) for high 8bit*/
			set_value = ((new_threshold-ONOFF_OFFSET) & 0xFF00)
						>> 8;
			break;

		case PS_HT_LSB:
			/*PS mode HTH(Lon) for low 8bit*/
			set_value = new_threshold & 0x00FF;
			break;

		case PS_HT_MSB:
			/* PS mode HTH(Lon) for high 8bit*/
			set_value = (new_threshold & 0xFF00) >> 8;
			break;

		default:
			continue;
		}

		if (update_reg)
			err = opt_i2c_write(selected_image[i][0], &set_value);

		if (err) {
			pr_err("%s : setting error i = %d, err=%d\n",
			 __func__, i, err);
			return err;
		} else
			selected_image[i][1] = set_value;
	}

	return err;
}

static int proximity_get_threshold(int onoff)
{
	int i;
	int threshold = 0;
	int ps_lsb_reg = PS_LT_LSB;
	int ps_msb_reg = PS_LT_MSB;

	if (onoff) {
		ps_lsb_reg = PS_HT_LSB;
		ps_msb_reg = PS_HT_MSB;
	}

	/* update threshold */
	for (i = 0; i < COL; i++) {
		if (gp2a_original_image[i][0] == ps_lsb_reg)
			/*PS mode LTH(Loff) */
			threshold = gp2a_original_image[i][1];
		else if (gp2a_original_image[i][0] == ps_msb_reg)
			/*PS mode LTH(Loff) */
			threshold |= gp2a_original_image[i][1]<<8;
	}
	pr_info("[%s] threshold is %d\n", __func__, threshold);

	return threshold;
}

static int proximity_open_offset(struct gp2a_data *data)
{
	struct file *offset_filp = NULL;
	mm_segment_t old_fs;
	int err = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	offset_filp = filp_open(OFFSET_FILE_PATH, O_RDONLY, 0666);

	if (IS_ERR(offset_filp)) {
		pr_err("%s: no offset file\n", __func__);
		err = PTR_ERR(offset_filp);
		if (err != -ENOENT)
			pr_err("%s: Can't open cancelation file\n", __func__);
		set_fs(old_fs);
		return err;
	}

	err = offset_filp->f_op->read(offset_filp,
		(char *)&data->offset_value, sizeof(u8), &offset_filp->f_pos);
	if (err != sizeof(u8)) {
		pr_err("%s: Can't read the cancel data from file\n", __func__);
		err = -EIO;
	}

	pr_info("%s: data->offset_value = %d\n",
		__func__, data->offset_value);
	filp_close(offset_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int proximity_store_offset(struct gp2a_data *gp2a)
{
	struct file *offset_filp = NULL;
	mm_segment_t old_fs;
	int err = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	offset_filp = filp_open(OFFSET_FILE_PATH,
	O_CREAT | O_TRUNC | O_WRONLY, 0666);

	if (IS_ERR(offset_filp)) {
		pr_err("%s: Can't open prox_offset file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(offset_filp);
		return err;
	}
	err = offset_filp->f_op->write(offset_filp,
	(char *)&gp2a->offset_value, sizeof(u8), &offset_filp->f_pos);

	if (err != sizeof(u8)) {
		pr_err("%s: Can't write the offset data to file\n", __func__);
		err = -EIO;
	}
	filp_close(offset_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int proximity_avg_adc_read(struct gp2a_data *gp2a)
{
	int adc_sum = 0;
	int adc_avg = 0;
	int D2_data = 0;
	int i = 0;
	unsigned char get_D2_data[2] = {0,};

	mutex_lock(&gp2a->data_mutex);
	for (i = 0; i < OFFSET_AVERAGE_LENGTH; i++) {
		mdelay(50);
		opt_i2c_read(0x10, get_D2_data, sizeof(get_D2_data));
		D2_data = (get_D2_data[1] << 8) | get_D2_data[0];

		adc_sum += D2_data;
	}
	mutex_unlock(&gp2a->data_mutex);

	adc_avg = (int)(adc_sum / OFFSET_AVERAGE_LENGTH);
	pr_info("%s: offset = %d\n", __func__, adc_avg);

	return adc_avg;
}

static int proximity_do_calibration(struct device *dev, bool do_calib)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	int avg_ps_data = 0;
	int threshold = 0;
	int err = 0;

	if (do_calib) {
		avg_ps_data = proximity_avg_adc_read(gp2a);
		/* Final Threshold : FX,Initial Threhold : X, Cross Talk : Y
		* FX = X + Y - (X - 8) ==> FX = Y + 8 (FX > X) */
		gp2a->offset_value = avg_ps_data - (gp2a->orig_on_threshold
						- gp2a->pdata->base_threshold);
		if (gp2a->offset_value < 0)
			gp2a->offset_value = 0;
	} else {
		gp2a->offset_value = 0;
		/* Default threshold */
		gp2a->orig_on_threshold  = gp2a->pdata->gp2a_get_threshold();
	}
	threshold = gp2a->orig_on_threshold + gp2a->offset_value;

	pr_info("[%s] New threshold : %d offset value: %d\n", __func__,
		threshold, gp2a->offset_value);
	err = gp2a_update_threshold(gp2a_original_image, threshold, true);

	if (err) {
		pr_err("%s : gp2a threshold(with register) update fail.\n",
			__func__);
		return err;
	}

	err = proximity_store_offset(gp2a);

	return err;
}

static ssize_t
proximity_enable_show(struct device *dev,
		      struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->proximity_enabled);
}

static ssize_t
proximity_enable_store(struct device *dev,
		       struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	int threshold;
	int value = 0;
	char input;
	int err = 0;

	err = kstrtoint(buf, 10, &value);

	if (err)
		pr_err("%s, kstrtoint failed.", __func__);

	if (value != 0 && value != 1)
		return count;

	pr_info("%s, %d value = %d\n", __func__, __LINE__, value);

	proximity_open_offset(data);

	if (data->proximity_enabled && !value) {	/* Prox power off */
		disable_irq(data->irq);

		proximity_onoff(0, data);
		disable_irq_wake(data->irq);

		if (data->pdata->gp2a_led_on)
			data->pdata->gp2a_led_on(0);
	}
	if (!data->proximity_enabled && value) {	/* prox power on */
		if (data->pdata->gp2a_led_on)
			data->pdata->gp2a_led_on(1);

		proximity_onoff(1, data);

		threshold = data->orig_on_threshold + data->offset_value;
		err = gp2a_update_threshold(gp2a_original_image,
			threshold, true);

		if (err) {
			pr_err("%s : gp2a threshold(with register) update fail.\n",
				__func__);
			proximity_onoff(0, data);
			return err;
		}

		enable_irq_wake(data->irq);
		msleep(160);

		input = gpio_get_value_cansleep(data->pdata->p_out);
		input_report_abs(data->proximity_input_dev,
			ABS_DISTANCE, input);
		input_sync(data->proximity_input_dev);

		enable_irq(data->irq);
	}
	data->proximity_enabled = value;

	return count;
}

static ssize_t proximity_state_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	static int count;		/*count for proximity average */

	int D2_data = 0;
	unsigned char get_D2_data[2] = { 0, };

	mutex_lock(&data->data_mutex);
	opt_i2c_read(0x10, get_D2_data, sizeof(get_D2_data));
	mutex_unlock(&data->data_mutex);
	D2_data = (get_D2_data[1] << 8) | get_D2_data[0];

	data->average[count] = D2_data;
	count++;
	if (count == PROX_READ_NUM)
		count = 0;

	return snprintf(buf, PAGE_SIZE, "%d\n", D2_data);
}

static ssize_t proximity_avg_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	int min = 0, max = 0, avg = 0;
	int i;
	int proximity_value = 0;

	for (i = 0; i < PROX_READ_NUM; i++) {
		proximity_value = data->average[i];
		if (proximity_value > 0) {

			avg += proximity_value;

			if (!i)
				min = proximity_value;
			else if (proximity_value < min)
				min = proximity_value;

			if (proximity_value > max)
				max = proximity_value;
		}
	}
	avg /= i;

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d\n", min, avg, max);
}

static ssize_t proximity_avg_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	bool new_value;
	int i = 0;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (new_value && !data->proximity_average_on) {
		if (!(data->proximity_enabled)) {
			/*data->pdata->gp2a_led_on(true);*/
			proximity_onoff(1, data);
		}
		data->proximity_average_on = 1;
	} else if (!new_value && data->proximity_average_on) {
		data->proximity_average_on = 0;

		if (!(data->proximity_enabled)) {
			proximity_onoff(0, data);
			/*data->pdata->gp2a_led_on(false);*/
		}

		for (i = 0 ; i < 3 ; i++)
			data->average[i] = 0;
	}

	return size;
}

static ssize_t proximity_raw_data_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	static int count;		/*count for proximity average */

	int D2_data = 0;
	unsigned char get_D2_data[2] = { 0, };

	mutex_lock(&data->data_mutex);
	opt_i2c_read(0x10, get_D2_data, sizeof(get_D2_data));
	mutex_unlock(&data->data_mutex);
	D2_data = (get_D2_data[1] << 8) | get_D2_data[0];

	data->average[count] = D2_data;
	count++;
	if (count == PROX_READ_NUM)
		count = 0;

	return sprintf(buf, "%d\n", D2_data);
}

static ssize_t proximity_thresh_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int threshold = 0;

	threshold = proximity_get_threshold(1);

	return sprintf(buf, "prox_threshold = %d\n", threshold);
}

static ssize_t proximity_thresh_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	unsigned long threshold;
	int err = 0;

	err = strict_strtoul(buf, 10, &threshold);

	if (err) {
		pr_err("%s, conversion %s to number.\n",
			__func__, buf);
		return err;
	}
	err = gp2a_update_threshold(gp2a_original_image,
			threshold, true);

	if (err) {
		pr_err("gp2a threshold(with register) update fail.\n");
		return err;
	}
	gp2a->orig_on_threshold = threshold - gp2a->offset_value;

	return size;
}

static ssize_t proximity_cal_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	int threshold = 0;

	threshold = proximity_get_threshold(1);

	pr_info("[%s] offset:%d, on threshold:%d\n", __func__,
		gp2a->offset_value, threshold);

	return sprintf(buf, "%d,%d\n", gp2a->offset_value,
				threshold);
}

static ssize_t proximity_cal_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	bool do_calib;
	int err = 0;

	if (sysfs_streq(buf, "1")) { /* calibrate cancelation value */
		do_calib = true;
	} else if (sysfs_streq(buf, "0")) { /* reset cancelation value */
		do_calib = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("[%s] is called %d\n", __func__, do_calib);

	err = proximity_do_calibration(dev, do_calib);
	if (err < 0) {
		pr_err("%s: proximity_do_calibration() failed\n", __func__);
		return err;
	}

	return size;
}

/* Light Sysfs interface */
static ssize_t lightsensor_file_state_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	int adc = 0;
	struct gp2a_data *data = dev_get_drvdata(dev);

	adc = lightsensor_get_adcvalue(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", adc);
}

static ssize_t lightsensor_raw_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	unsigned char get_data[4] = { 0, };
	int D0_raw_data;
	int D1_raw_data;
	int ret = 0;

	mutex_lock(&data->data_mutex);
	ret = opt_i2c_read(DATA0_LSB, get_data, sizeof(get_data));
	mutex_unlock(&data->data_mutex);
	if (ret < 0)
		pr_err("%s i2c err: %d\n", __func__, ret) ;
	D0_raw_data = (get_data[1] << 8) | get_data[0];	/* clear */
	D1_raw_data = (get_data[3] << 8) | get_data[2];	/* IR */

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", D0_raw_data, D1_raw_data);
}

static ssize_t
light_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->light_delay);
}

static ssize_t
light_delay_store(struct device *dev, struct device_attribute *attr,
		  const char *buf, size_t count)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	int delay;
	int err = 0;

	err = kstrtoint(buf, 10, &delay);

	if (err)
		pr_err("%s, kstrtoint failed.", __func__);

	if (delay < 0)
		return count;

	delay = delay / 1000000;	/* ns to msec */

	pr_info("%s, new_delay = %d, old_delay = %d", __func__, delay,
	       data->light_delay);

	if (SENSOR_MAX_DELAY < delay)
		delay = SENSOR_MAX_DELAY;

	data->light_delay = delay;

	mutex_lock(&data->light_mutex);

	if (data->light_enabled) {
		cancel_delayed_work_sync(&data->light_work);
		schedule_delayed_work(&data->light_work,
			msecs_to_jiffies(delay));
	}

	/*input_report_abs(input_data, ABS_CONTROL_REPORT
	   , (data->delay<<16) | delay); */

	mutex_unlock(&data->light_mutex);

	return count;
}

static ssize_t
light_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->light_enabled);
}

static ssize_t
light_enable_store(struct device *dev, struct device_attribute *attr,
		   const char *buf, size_t count)
{
	struct gp2a_data *data = dev_get_drvdata(dev);
	int value;
	int err = 0;

	err = kstrtoint(buf, 10, &value);

	if (err)
		pr_err("%s, kstrtoint failed.", __func__);

	pr_debug("%s, %d value = %d\n", __func__, __LINE__, value);

	if (value != 0 && value != 1)
		return count;

	mutex_lock(&data->light_mutex);
	if (data->light_enabled && !value) {
		cancel_delayed_work_sync(&data->light_work);
		lightsensor_onoff(0, data);
		data->light_enabled = value;
	}
	if (!data->light_enabled && value) {
		data->light_data_first = true;
		lightsensor_onoff(1, data);
		mdelay(50);
		data->light_enabled = value;
		schedule_delayed_work(&data->light_work, 0);
	}
	mutex_unlock(&data->light_mutex);

	return count;
}
static ssize_t gp2a_vendor_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", GP2A_VENDOR);
}
static ssize_t gp2a_name_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct gp2a_data *data = dev_get_drvdata(dev);

	return is_gp2a030a(data) ? sprintf(buf, "%s030\n", GP2A_CHIP_ID)
		: sprintf(buf, "%s020\n", GP2A_CHIP_ID);
}

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	       proximity_enable_show, proximity_enable_store);
static DEVICE_ATTR(prox_avg, S_IRUGO|S_IWUSR, proximity_avg_show,
	proximity_avg_store);
static DEVICE_ATTR(state, S_IRUGO|S_IWUSR, proximity_state_show, NULL);
static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP, light_delay_show,
	light_delay_store);
static struct device_attribute attr_prox_raw = __ATTR(raw_data, 0644,
	proximity_raw_data_show, NULL);
static DEVICE_ATTR(prox_thresh, S_IRUGO | S_IWUSR,
			proximity_thresh_show, proximity_thresh_store);
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR, proximity_cal_show,
	proximity_cal_store);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	       light_enable_show, light_enable_store);
static DEVICE_ATTR(lux, S_IRUGO|S_IWUSR, lightsensor_file_state_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, lightsensor_raw_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO|S_IWUSR, gp2a_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO|S_IWUSR, gp2a_name_show, NULL);

static struct attribute *proximity_attributes[] = {
	&dev_attr_proximity_enable.attr,
	&dev_attr_state.attr,
	&dev_attr_prox_thresh.attr,
	NULL
};

static struct attribute *lightsensor_attributes[] = {
	&dev_attr_poll_delay.attr,
	&dev_attr_light_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_attributes
};

static struct attribute_group lightsensor_attribute_group = {
	.attrs = lightsensor_attributes
};

irqreturn_t gp2a_irq_handler(int irq, void *dev_id)
{
	struct gp2a_data *gp2a = dev_id;
	wake_lock_timeout(&gp2a->prx_wake_lock, 3 * HZ);

#ifdef CONFIG_SLP
	pm_wakeup_event(gp2a->proximity_dev, 0);
#endif

	schedule_work(&gp2a->proximity_work);

	pr_debug("[PROXIMITY] IRQ_HANDLED.\n");
	return IRQ_HANDLED;
}

static int gp2a_setup_irq(struct gp2a_data *gp2a)
{
	int rc = -EIO;
	struct gp2a_platform_data *pdata = gp2a->pdata;
	int irq;

	pr_info("%s, start\n", __func__);

	rc = gpio_request(pdata->p_out, "gpio_proximity_out");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
		       __func__, pdata->p_out, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->p_out);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
		       __func__, pdata->p_out, rc);
		goto err_gpio_direction_input;
	}

	irq = gpio_to_irq(pdata->p_out);
	pr_info("%s: irq :%d\n", __func__, pdata->p_out);
	rc = request_threaded_irq(irq, NULL,
				  gp2a_irq_handler,
				  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				  "proximity_int", gp2a);
	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
		       __func__, irq, pdata->p_out, rc);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(irq);
	gp2a->irq = irq;

	pr_err("%s, success\n", __func__);

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(pdata->p_out);
done:
	return rc;
}

static void gp2a_work_func_prox(struct work_struct *work)
{
	struct gp2a_data *gp2a = container_of((struct work_struct *)work,
					      struct gp2a_data, proximity_work);
	unsigned char value;
	char result;
	int ret;

	/* 0 : proximity, 1 : away */
	result = gpio_get_value_cansleep(gp2a->pdata->p_out);
	gp2a->proximity_detection = !result;

	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, result);
	pr_info("%s Proximity values = %d\n", __func__, result);
	input_sync(gp2a->proximity_input_dev);

	disable_irq(gp2a->irq);

	value = 0x0C;
	ret = opt_i2c_write(COMMAND1, &value);	/*Software reset */

	if (result == 0) {	/* detection = Falling Edge */
		if (gp2a->lightsensor_mode == 0)	/* Low mode */
			value = 0x23;
		else		/* High mode */
			value = 0x27;
		ret = opt_i2c_write(COMMAND2, &value);
	} else {		/* none Detection */
		if (gp2a->lightsensor_mode == 0)	/* Low mode */
			value = 0x63;
		else		/* High mode */
			value = 0x67;
		ret = opt_i2c_write(COMMAND2, &value);
	}

	enable_irq(gp2a->irq);

	value = 0xCC;
	ret = opt_i2c_write(COMMAND1, &value);

	gp2a->prox_data = result;
	pr_debug("proximity = %d, lightsensor_mode=%d\n",
		result, gp2a->lightsensor_mode);
}

int opt_i2c_read(u8 reg, unsigned char *rbuf, int len)
{
	int ret = -1;
	struct i2c_msg msg;
	/*int i;*/

	if ((opt_i2c_client == NULL) || (!opt_i2c_client->adapter)) {
		printk(KERN_ERR "%s %d (opt_i2c_client == NULL)\n",
		       __func__, __LINE__);
		return -ENODEV;
	}

	/*gprintk("register num : 0x%x\n", reg); */
	msg.addr = opt_i2c_client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 1;
	msg.buf = &reg;

	ret = i2c_transfer(opt_i2c_client->adapter, &msg, 1);

	if (ret >= 0) {
		msg.flags = I2c_M_RD;
		msg.len = len;
		msg.buf = rbuf;
		ret = i2c_transfer(opt_i2c_client->adapter, &msg, 1);
	}

	if (ret < 0)
		printk(KERN_ERR "%s, i2c transfer error ret=%d\n"\
		, __func__, ret);

	/* for (i=0;i<len;i++)
		gprintk("0x%x, 0x%x\n", reg++,rbuf[i]); */

	return ret;
}

int opt_i2c_write(u8 reg, u8 *val)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 3;

	if ((opt_i2c_client == NULL) || (!opt_i2c_client->adapter))
		return -ENODEV;

	while (retry--) {
		data[0] = reg;
		data[1] = *val;

		msg->addr = opt_i2c_client->addr;
		msg->flags = I2C_M_WR;
		msg->len = 2;
		msg->buf = data;

		err = i2c_transfer(opt_i2c_client->adapter, msg, 1);
		/*gprintk("0x%x, 0x%x\n", reg, *val); */

		if (err >= 0)
			return 0;
	}
	printk(KERN_ERR "%s, i2c transfer error(%d)\n", __func__, err);
	return err;
}

static int proximity_input_init(struct gp2a_data *data)
{
	struct input_dev *dev;
	int err = 0;

	pr_info("%s, %d start\n", __func__, __LINE__);

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s, error\n", __func__);
		return -ENOMEM;
	}

	input_set_capability(dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(dev, ABS_DISTANCE, 0, 1, 0, 0);

	dev->name = "proximity_sensor";
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	data->proximity_input_dev = dev;

	pr_info("%s, success\n", __func__);
	return 0;
}

static int light_input_init(struct gp2a_data *data)
{
	struct input_dev *dev;
	int err = 0;

	pr_info("%s, %d start\n", __func__, __LINE__);

	dev = input_allocate_device();
	if (!dev) {
		pr_err("%s, error\n", __func__);
		return -ENOMEM;
	}

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_MISC, 0, 1, 0, 0);

	dev->name = "light_sensor";
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	data->light_input_dev = dev;

	pr_info("%s, success\n", __func__);
	return 0;
}

int lightsensor_get_adc(struct gp2a_data *data)
{
	unsigned char get_data[4] = { 0, };
	int D0_raw_data;
	int D1_raw_data;
	int D0_data;
	int D1_data;
	int lx = 0;
	u8 value;
	int light_alpha;
	int light_beta;
	static int lx_prev;
	int ret = 0;
	int d0_boundary = 92;
	mutex_lock(&data->data_mutex);
	ret = opt_i2c_read(DATA0_LSB, get_data, sizeof(get_data));
	mutex_unlock(&data->data_mutex);
	if (ret < 0)
		return lx_prev;
	D0_raw_data = (get_data[1] << 8) | get_data[0];	/* clear */
	D1_raw_data = (get_data[3] << 8) | get_data[2];	/* IR */
	if (is_gp2a030a(data)) {  /* GP2AP 030 */
		if (100 * D1_raw_data <= 41 * D0_raw_data) {
			light_alpha = 728;
			light_beta = 0;
		} else if (100 * D1_raw_data <= 62 * D0_raw_data) {
			light_alpha = 1660;
			light_beta = 2274;
		} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
			light_alpha = 766;
			light_beta = 832;
		} else {
			light_alpha = 0;
			light_beta = 0;
		}
	} else {   /* GP2AP 020 */
		if (data->lightsensor_mode) {	/* HIGH_MODE */
			if (100 * D1_raw_data <= 32 * D0_raw_data) {
				light_alpha = 800;
				light_beta = 0;
			} else if (100 * D1_raw_data <= 67 * D0_raw_data) {
				light_alpha = 2015;
				light_beta = 2925;
			} else if (100 * D1_raw_data <=
				d0_boundary * D0_raw_data) {
				light_alpha = 56;
				light_beta = 12;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		} else {		/* LOW_MODE */
			if (100 * D1_raw_data <= 32 * D0_raw_data) {
				light_alpha = 800;
				light_beta = 0;
			} else if (100 * D1_raw_data <= 67 * D0_raw_data) {
				light_alpha = 2015;
				light_beta = 2925;
			} else if (100 * D1_raw_data <=
				d0_boundary * D0_raw_data) {
				light_alpha = 547;
				light_beta = 599;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		}
	}

	if (data->lightsensor_mode) {	/* HIGH_MODE */
		D0_data = D0_raw_data * 16;
		D1_data = D1_raw_data * 16;
	} else {		/* LOW_MODE */
		D0_data = D0_raw_data;
		D1_data = D1_raw_data;
	}
	if (is_gp2a030a(data)) {  /* GP2AP 030 */

		if (D0_data < 3) {

			lx = 0;
		} else if (data->lightsensor_mode == 0
			   && (D0_raw_data >= 16000 || D1_raw_data >= 16000)
			   && (D0_raw_data <= 16383 && D1_raw_data <= 16383)) {
			lx = lx_prev;
		} else if (100 * D1_data > d0_boundary * D0_data) {

			lx = lx_prev;
			return lx;
		} else {
			lx = (int)((light_alpha / 10 * D0_data * 33)
				- (light_beta / 10 * D1_data * 33)) / 1000;
		}
	} else {   /* GP2AP 020 */
		if ((D0_data == 0 || D1_data == 0)\
			&& (D0_data < 300 && D1_data < 300)) {
			lx = 0;
		} else if (data->lightsensor_mode == 0
			   && (D0_raw_data >= 16000 || D1_raw_data >= 16000)
			   && (D0_raw_data <= 16383 && D1_raw_data <= 16383)) {
			lx = lx_prev;

		} else if ((100 * D1_data > d0_boundary * D0_data)
			   || (100 * D1_data < 15 * D0_data)) {
			lx = lx_prev;
			return lx;
		} else {
			lx = (int)((light_alpha / 10 * D0_data * 33)
				- (light_beta / 10 * D1_data * 33)) / 1000;
		}
	}

	lx_prev = lx;

	if (data->lightsensor_mode) {	/* HIGH MODE */
		if (D0_raw_data < 1000) {
			pr_info("%s: change to LOW_MODE detection=%d\n",
			       __func__, data->proximity_detection);
			data->lightsensor_mode = 0;	/* change to LOW MODE */

			value = 0x0C;
			opt_i2c_write(COMMAND1, &value);

			if (data->proximity_detection)
				value = 0x23;
			else
				value = 0x63;
			opt_i2c_write(COMMAND2, &value);

			if (data->proximity_enabled)
				value = 0xCC;
			else
				value = 0xDC;
			opt_i2c_write(COMMAND1, &value);
		}
	} else {		/* LOW MODE */
		if (D0_raw_data > 16000 || D1_raw_data > 16000) {
			pr_info("%s: change to HIGH_MODE detection=%d\n",
			       __func__, data->proximity_detection);
			/* change to HIGH MODE */
			data->lightsensor_mode = 1;

			value = 0x0C;
			opt_i2c_write(COMMAND1, &value);

			if (data->proximity_detection)
				value = 0x27;
			else
				value = 0x67;
			opt_i2c_write(COMMAND2, &value);

			if (data->proximity_enabled)
				value = 0xCC;
			else
				value = 0xDC;
			opt_i2c_write(COMMAND1, &value);
		}
	}

	return lx;
}

static int lightsensor_get_adcvalue(struct gp2a_data *data)
{
	int i = 0, j = 0;
	unsigned int adc_total = 0;
	static int adc_avr_value;
	unsigned int adc_index = 0;
	static unsigned int adc_index_count;
	unsigned int adc_max = 0;
	unsigned int adc_min = 0;
	int value = 0;
	static int adc_value_buf[ADC_BUFFER_NUM] = { 0 };

	value = lightsensor_get_adc(data);

	/*cur_adc_value = value; */

	adc_index = (adc_index_count++) % ADC_BUFFER_NUM;

	/*ADC buffer initialize (light sensor off ---> light sensor on) */
	if (data->light_data_first) {
		for (j = 0; j < ADC_BUFFER_NUM; j++)
			adc_value_buf[j] = value;
		data->light_data_first = false;
	} else {
		adc_value_buf[adc_index] = value;
	}

	adc_max = adc_value_buf[0];
	adc_min = adc_value_buf[0];

	for (i = 0; i < ADC_BUFFER_NUM; i++) {
		adc_total += adc_value_buf[i];

		if (adc_max < adc_value_buf[i])
			adc_max = adc_value_buf[i];

		if (adc_min > adc_value_buf[i])
			adc_min = adc_value_buf[i];
	}
	adc_avr_value =
	    (adc_total - (adc_max + adc_min)) / (ADC_BUFFER_NUM - 2);

	if (adc_index_count == ADC_BUFFER_NUM - 1)
		adc_index_count = 0;

	return adc_avr_value;
}

static int lightsensor_onoff(u8 onoff, struct gp2a_data *data)
{
	u8 value;

#ifdef DEBUG
	pr_info(KERN_INFO "%s : light_sensor onoff = %d\n", __func__, onoff);
	       data->proximity_enabled);
#endif

	if (onoff) {
		/*in calling, must turn on proximity sensor */
		if (data->proximity_enabled == 0) {
			value = 0x01;
			opt_i2c_write(COMMAND4, &value);
			value = 0x63;
			opt_i2c_write(COMMAND2, &value);
			/*OP3 : 1(operating mode) OP2 :1
			   (coutinuous operating mode)
			   OP1 : 01(ALS mode) TYPE=0(auto) */
			value = 0xD0;
			opt_i2c_write(COMMAND1, &value);
			/* other setting have defualt value. */
		}
	} else {
		/*in calling, must turn on proximity sensor */
		if (data->proximity_enabled == 0) {
			value = 0x00;	/*shutdown mode */
			opt_i2c_write((u8) (COMMAND1), &value);
		}
	}

	return 0;
}

static void gp2a_work_func_light(struct work_struct *work)
{
	struct gp2a_data *data = container_of((struct delayed_work *)work,
						struct gp2a_data, light_work);
	int adc = 0;

	adc = lightsensor_get_adcvalue(data);

	input_report_abs(data->light_input_dev, ABS_MISC, adc);
	input_sync(data->light_input_dev);

	if (data->light_enabled)
		schedule_delayed_work(&data->light_work,
			msecs_to_jiffies(data->light_delay));
}


static int gp2a_opt_probe(struct platform_device *pdev)
{
	struct gp2a_data *gp2a;
	struct gp2a_platform_data *pdata = pdev->dev.platform_data;
	u8 value = 0;
	int err = 0;

	pr_info("%s : probe start!\n", __func__);

	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		return err;
	}

	if (!pdata->gp2a_led_on) {
		pr_err("%s: incomplete pdata!\n", __func__);
		return err;
	}

	/* gp2a power on */
	pdata->gp2a_led_on(true);
	pr_info("%s: irq:%d\n", __func__, pdata->p_out);

	/* allocate driver_data */
	gp2a = kzalloc(sizeof(struct gp2a_data), GFP_KERNEL);
	if (!gp2a) {
		pr_err("kzalloc error\n");
		return -ENOMEM;

	}
	gp2a->proximity_enabled = 0;
	gp2a->offset_value = 0;
	gp2a->pdata = pdata;

	gp2a->light_enabled = 0;
	gp2a->light_delay = SENSOR_DEFAULT_DELAY;

	if (is_gp2a030a(gp2a)) {
		pr_info("%s : gp2ap030 is used\n", __func__);
		gp2a_original_image[1][1] = 0x1A;
		gp2a_original_image[3][1] = 0x0A;
		gp2a_original_image[5][1] = 0x0A;
	}

	if (pdata->gp2a_get_threshold) {
		gp2a_update_threshold(gp2a_original_image,
			pdata->gp2a_get_threshold(), false);
	}
	gp2a->orig_on_threshold = proximity_get_threshold(1);

	INIT_DELAYED_WORK(&gp2a->light_work, gp2a_work_func_light);
	INIT_WORK(&gp2a->proximity_work, gp2a_work_func_prox);

	err = proximity_input_init(gp2a);
	if (err < 0)
		goto error_setup_reg_prox;

	err = light_input_init(gp2a);
	if (err < 0)
		goto error_setup_reg_light;

	err = sysfs_create_group(&gp2a->proximity_input_dev->dev.kobj,
				 &proximity_attribute_group);
	if (err < 0)
		goto err_sysfs_create_group_proximity;

	err = sysfs_create_group(&gp2a->light_input_dev->dev.kobj,
				&lightsensor_attribute_group);
	if (err)
		goto err_sysfs_create_group_light;

	mutex_init(&gp2a->light_mutex);
	mutex_init(&gp2a->data_mutex);

	/* set platdata */
	platform_set_drvdata(pdev, gp2a);

	/* wake lock init */
	wake_lock_init(&gp2a->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");

	/* init i2c */
	opt_i2c_init();

	/* GP2A Regs INIT SETTINGS  and Check I2C communication */

	/* shutdown mode op[3]=0 */
	value = 0x00;
	err = opt_i2c_write((u8) (COMMAND1), &value);
	if (err < 0) {
		pr_err("%s failed : threre is no such device.\n", __func__);
		goto err_no_device;
	}

	/* Setup irq */
	err = gp2a_setup_irq(gp2a);
	if (err) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	/* set sysfs for proximity sensor */
	gp2a->proximity_dev = sensors_classdev_register("proximity_sensor");
	if (IS_ERR(gp2a->proximity_dev)) {
		pr_err("%s: could not create proximity_dev\n", __func__);
		goto err_proximity_device_create;
	}

	gp2a->light_dev = sensors_classdev_register("light_sensor");
	if (IS_ERR(gp2a->light_dev)) {
		pr_err("%s: could not create light_dev\n", __func__);
		goto err_light_device_create;
	}

	if (device_create_file(gp2a->proximity_dev, &dev_attr_state) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_state.attr.name);
		goto err_proximity_device_create_file1;
	}

	if (device_create_file(gp2a->proximity_dev, &dev_attr_prox_avg) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_prox_avg.attr.name);
		goto err_proximity_device_create_file2;
	}

	if (device_create_file(gp2a->proximity_dev,
						&dev_attr_prox_thresh) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_prox_thresh.attr.name);
		goto err_proximity_device_create_file3;
	}

	if (device_create_file(gp2a->proximity_dev, &attr_prox_raw) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       attr_prox_raw.attr.name);
		goto err_proximity_device_create_file4;
	}

	if (device_create_file(gp2a->proximity_dev,
		&dev_attr_proximity_enable) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_proximity_enable.attr.name);
		goto err_proximity_device_create_file5;
	}

	if (is_gp2a030a(gp2a)) {
		if (device_create_file(gp2a->proximity_dev,
			&dev_attr_prox_cal) < 0) {
			pr_err("%s: could not create device file(%s)!\n",
				__func__, dev_attr_prox_cal.attr.name);
			goto err_proximity_device_create_file6;
		}
	}

	if (device_create_file(gp2a->proximity_dev,
		&dev_attr_vendor) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_vendor.attr.name);
		goto err_proximity_device_create_file7;
	}

	if (device_create_file(gp2a->proximity_dev,
		&dev_attr_name) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_name.attr.name);
		goto err_proximity_device_create_file8;
	}

	if (device_create_file(gp2a->light_dev, &dev_attr_lux) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_lux.attr.name);
		goto err_light_device_create_file1;
	}

	if (device_create_file(gp2a->light_dev, &dev_attr_light_enable) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_light_enable.attr.name);
		goto err_light_device_create_file2;
	}

	if (device_create_file(gp2a->light_dev,
		&dev_attr_vendor) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_vendor.attr.name);
		goto err_light_device_create_file3;
	}

	if (device_create_file(gp2a->light_dev,
		&dev_attr_name) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_name.attr.name);
		goto err_light_device_create_file4;
	}

	if (device_create_file(gp2a->light_dev,
		&dev_attr_raw_data) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
		       dev_attr_name.attr.name);
		goto err_light_device_create_file5;
	}

#ifdef CONFIG_SLP
#ifdef CONFIG_SLP_WAKEUP_COUNT
	device_init_wakeup_setirq(&pdev->dev, gp2a->irq);
#endif
	device_init_wakeup(gp2a->proximity_dev, true);
#endif

	dev_set_drvdata(gp2a->proximity_dev, gp2a);
	dev_set_drvdata(gp2a->light_dev, gp2a);

#ifdef CONFIG_SLP_WAKEUP_COUNT
	device_init_wakeup_setirq(&pdev->dev, gp2a->irq);
#else
	device_init_wakeup(&pdev->dev, 1);
#endif

	pr_info("%s : probe success!\n", __func__);

	return 0;

err_light_device_create_file5:
	device_remove_file(gp2a->light_dev, &dev_attr_name);
err_light_device_create_file4:
	device_remove_file(gp2a->light_dev, &dev_attr_vendor);
err_light_device_create_file3:
	device_remove_file(gp2a->light_dev, &dev_attr_light_enable);
err_light_device_create_file2:
	device_remove_file(gp2a->light_dev, &dev_attr_lux);
err_light_device_create_file1:
	device_remove_file(gp2a->proximity_dev, &dev_attr_name);
err_proximity_device_create_file8:
	device_remove_file(gp2a->proximity_dev, &dev_attr_vendor);
err_proximity_device_create_file7:
	if (is_gp2a030a(gp2a))
		device_remove_file(gp2a->proximity_dev, &dev_attr_prox_cal);
err_proximity_device_create_file6:
	device_remove_file(gp2a->proximity_dev, &dev_attr_proximity_enable);
err_proximity_device_create_file5:
	device_remove_file(gp2a->proximity_dev, &attr_prox_raw);
err_proximity_device_create_file4:
	device_remove_file(gp2a->proximity_dev, &dev_attr_prox_thresh);
err_proximity_device_create_file3:
	device_remove_file(gp2a->proximity_dev, &dev_attr_prox_avg);
err_proximity_device_create_file2:
	device_remove_file(gp2a->proximity_dev, &dev_attr_state);
err_proximity_device_create_file1:
err_light_device_create:
err_proximity_device_create:
	gpio_free(pdata->p_out);
err_setup_irq:
err_no_device:
	wake_lock_destroy(&gp2a->prx_wake_lock);
	mutex_destroy(&gp2a->light_mutex);
	mutex_destroy(&gp2a->data_mutex);
	sysfs_remove_group(&gp2a->light_input_dev->dev.kobj,
			   &lightsensor_attribute_group);
err_sysfs_create_group_light:
	sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
err_sysfs_create_group_proximity:
	input_unregister_device(gp2a->light_input_dev);
error_setup_reg_light:
	input_unregister_device(gp2a->proximity_input_dev);
error_setup_reg_prox:
	kfree(gp2a);
	return err;
}

static int gp2a_opt_remove(struct platform_device *pdev)
{
	struct gp2a_data *gp2a = platform_get_drvdata(pdev);

	if (gp2a == NULL) {
		pr_err("%s, gp2a_data is NULL!!!!!\n", __func__);
		return 0;
	}


	if (gp2a->proximity_enabled) {
		disable_irq(gp2a->irq);
		proximity_onoff(0, gp2a);
		disable_irq_wake(gp2a->irq);
		gp2a->pdata->gp2a_led_on(false);
		gp2a->proximity_enabled = 0;
	}

#ifdef CONFIG_SLP
	device_init_wakeup(gp2a->proximity_dev, false);
#endif

	device_remove_file(gp2a->proximity_dev, &dev_attr_state);
	device_remove_file(gp2a->proximity_dev, &dev_attr_prox_avg);
	device_remove_file(gp2a->proximity_dev, &dev_attr_prox_thresh);
	device_remove_file(gp2a->proximity_dev, &attr_prox_raw);
	device_remove_file(gp2a->proximity_dev, &dev_attr_proximity_enable);
	if (is_gp2a030a(gp2a))
		device_remove_file(gp2a->proximity_dev,
			&dev_attr_prox_cal);
	device_remove_file(gp2a->proximity_dev,	&dev_attr_vendor);
	device_remove_file(gp2a->proximity_dev,	&dev_attr_name);
	device_remove_file(gp2a->light_dev, &dev_attr_lux);
	device_remove_file(gp2a->light_dev, &dev_attr_light_enable);
	device_remove_file(gp2a->light_dev, &dev_attr_vendor);
	device_remove_file(gp2a->light_dev,	&dev_attr_name);
	device_remove_file(gp2a->light_dev, &dev_attr_raw_data);
	sensors_classdev_unregister(gp2a->proximity_dev);
	sensors_classdev_unregister(gp2a->light_dev);

	if (gp2a->proximity_input_dev != NULL) {
		sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
				   &proximity_attribute_group);
		input_unregister_device(gp2a->proximity_input_dev);

		if (gp2a->proximity_input_dev != NULL)
			kfree(gp2a->proximity_input_dev);
	}

	cancel_delayed_work(&gp2a->light_work);
	flush_scheduled_work();
	mutex_destroy(&gp2a->light_mutex);

	if (gp2a->light_input_dev != NULL) {
		sysfs_remove_group(&gp2a->light_input_dev->dev.kobj,
				   &lightsensor_attribute_group);
		input_unregister_device(gp2a->light_input_dev);

		if (gp2a->light_input_dev != NULL)
			kfree(gp2a->light_input_dev);
	}

	mutex_destroy(&gp2a->data_mutex);
	device_init_wakeup(&pdev->dev, 0);

	kfree(gp2a);

	return 0;
}

static int gp2a_opt_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gp2a_data *gp2a = platform_get_drvdata(pdev);

	mutex_lock(&gp2a->light_mutex);
	if (gp2a->light_enabled)
		cancel_delayed_work_sync(&gp2a->light_work);

	mutex_unlock(&gp2a->light_mutex);
	if (gp2a->proximity_enabled) {
		if (device_may_wakeup(&pdev->dev))
			enable_irq_wake(gp2a->irq);
	}

	return 0;
}

static int gp2a_opt_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gp2a_data *gp2a = platform_get_drvdata(pdev);

	if (gp2a->proximity_enabled) {
		if (device_may_wakeup(&pdev->dev))
			enable_irq_wake(gp2a->irq);
	}

	mutex_lock(&gp2a->light_mutex);
	if (gp2a->light_enabled)
		schedule_delayed_work(&gp2a->light_work, 0);
	mutex_unlock(&gp2a->light_mutex);

	return 0;
}

static int proximity_onoff(u8 onoff, struct gp2a_data  *data)
{
	u8 value;
	int i;
	/* unsigned char get_data[1]; */
	int err = 0;

	pr_info("%s : proximity turn on/off = %d\n", __func__, onoff);

	/* already on light sensor, so must simultaneously
	   turn on light sensor and proximity sensor */
	if (onoff) {
		for (i = 0; i < COL; i++) {
			err = opt_i2c_write(gp2a_original_image[i][0],
				&gp2a_original_image[i][1]);
			if (err < 0)
				pr_err("%s : turnning on error i = %d, err=%d\n",
				       __func__, i, err);
			data->lightsensor_mode = 0;
		}
	} else { /* light sensor turn on and proximity turn off */
		if (data->lightsensor_mode)
			value = 0x67; /*resolution :16bit, range: *8(HIGH) */
		else
			value = 0x63; /* resolution :16bit, range: *128(LOW) */
		opt_i2c_write(COMMAND2, &value);
		/* OP3 : 1(operating mode)
		   OP2 :1(coutinuous operating mode) OP1 : 01(ALS mode) */
		value = 0xD0;
		opt_i2c_write(COMMAND1, &value);
	}

	return 0;
}

static int opt_i2c_remove(struct i2c_client *client)
{
	kfree(client);
	opt_i2c_client = NULL;

	return 0;
}

static int opt_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	pr_info("%s start!!!\n", __func__);

	if (client == NULL)
		printk(KERN_ERR "GP2A i2c client is NULL !!!\n");

	opt_i2c_client = client;
	pr_info("%s end!!!\n", __func__);

	return 0;
}

static const struct i2c_device_id opt_device_id[] = {
	{"gp2a", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, opt_device_id);

static struct i2c_driver opt_i2c_driver = {
	.driver = {
		   .name = "gp2a",
		   .owner = THIS_MODULE,
		   },
	.probe = opt_i2c_probe,
	.remove = opt_i2c_remove,
	.id_table = opt_device_id,
};

static const struct dev_pm_ops gp2a_dev_pm_ops = {
	.suspend = gp2a_opt_suspend,
	.resume = gp2a_opt_resume,
};

static struct platform_driver gp2a_opt_driver = {
	.probe = gp2a_opt_probe,
	.remove = gp2a_opt_remove,
	.driver = {
		.name = "gp2a-opt",
		.owner = THIS_MODULE,
		.pm = &gp2a_dev_pm_ops,
	},
};

static int opt_i2c_init(void)
{
	if (i2c_add_driver(&opt_i2c_driver)) {
		pr_err("i2c_add_driver failed\n");
		return -ENODEV;
	}
	return 0;
}


static int __init gp2a_opt_init(void)
{
	int ret;

	ret = platform_driver_register(&gp2a_opt_driver);
	return ret;
}

static void __exit gp2a_opt_exit(void)
{
	platform_driver_unregister(&gp2a_opt_driver);
}

module_init(gp2a_opt_init);
module_exit(gp2a_opt_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for GP2AP020A00F");
MODULE_LICENSE("GPL");
