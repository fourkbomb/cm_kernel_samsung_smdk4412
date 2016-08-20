/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "ssp.h"

/* sensorhub ioctl command */
#define SENSORHUB_IOCTL_MAGIC	'S'
#define IOCTL_WRITE_CONTEXT_SIZE	_IOWR(SENSORHUB_IOCTL_MAGIC, 1, int)
#define IOCTL_WRITE_CONTEXT_DATA	_IOWR(SENSORHUB_IOCTL_MAGIC, 2, char *)
#define IOCTL_READ_CONTEXT_DATA		_IOR(SENSORHUB_IOCTL_MAGIC, 3, char *)


static int ssp_i2c_transfer(struct ssp_data *data,
			char *send_data, int send_length,
			char *receive_data, int receive_length)
{
	int retry = 2;
	int ret = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = data->client->addr,
			.flags = 0, /* write */
			.len = send_length,
			.buf = send_data,
		},
		{
			.addr = data->client->addr,
			.flags = I2C_M_RD, /* read */
			.len = receive_length,
			.buf = receive_data,
		},
	};

	/* execute both writing and reading at the same time */
	while (retry--) {
		ret = i2c_transfer(data->client->adapter, msgs, 2);
		if (ret == 2)
			return ret;
	}

	pr_err("%s: i2c err(%d)", __func__, ret);
	return ret;
}

static int ssp_send_data(struct ssp_data *data, char *send_buf, int length)
{
	char send_data[1 + length];
	char receive_data = 0;
	int ret = 0;
	int i;

	pr_err("%s: send data length = %d", __func__, length);
	for (i = 0; i < length; i++)
		pr_err("%s: send data[%d] = %d", __func__, i, send_buf[i]);

	send_data[0] = MSG2SSP_STS;
	send_data[1] = 1 + length; /* SSM + length */

	ret = ssp_i2c_transfer(data, send_data, 2, &receive_data, 1);
	if (ret < 0) {
		pr_err("%s: MSG2SSP_STS i2c err(%d)", __func__, ret);
		return ret;
	} else if (receive_data != MSG_ACK) {
		pr_err("%s: MSG2SSP_STS no ack err", __func__);
		return -EIO;
	}

	send_data[0] = MSG2SSP_SSM;
	memcpy(&send_data[1], send_buf, length);

	ret = ssp_i2c_transfer(data, send_data, 1 + length, &receive_data, 1);
	if (ret < 0) {
		pr_err("%s: MSG2SSP_SSM i2c err(%d)", __func__, ret);
	} else if (receive_data != MSG_ACK) {
		pr_err("%s: MSG2SSP_SSM no ack err", __func__);
		ret = -EIO;
	}

	return ret < 0 ? ret : length;
}

int ssp_receive_large_msg(struct ssp_data *data, u8 sub_cmd)
{
	char send_data[2] = { 0, };
	char receive_data[2] = { 0, };
	char *large_msg_data; /* Nth large msg data */
	int length = 0; /* length of Nth large msg */
	int context_data_locater = 0; /* context_data current position */
	int total_msg_number; /* total number of large msg */
	int msg_number; /* current number of large msg */
	int ret = 0;

	/* receive the first msg length */
	send_data[0] = MSG2SSP_STT;
	send_data[1] = sub_cmd;

	/* receive_data(msg length) is two byte because msg is large */
	ret = ssp_i2c_transfer(data, send_data, 2, receive_data, 2);
	if (ret < 0) {
		pr_err("%s: MSG2SSP_STT i2c err(%d)", __func__, ret);
		return ret;
	}

	/* get the first msg length */
	length = ((unsigned int)receive_data[0] << 8)
		+ (unsigned int)receive_data[1];
	if (length < 3) {
		/* do not print err message with power-up */
		if (sub_cmd != SUBCMD_POWEREUP)
			pr_err("%s: 1st large msg data not ready(length=%d)",
				__func__, length);
		return -EINVAL;
	}

	/* receive the first msg data */
	send_data[0] = MSG2SSP_SRM;
	large_msg_data = kzalloc((length  * sizeof(char)), GFP_KERNEL);
	ret = ssp_i2c_transfer(data, send_data, 1,
				large_msg_data, length);
	if (ret < 0) {
		pr_err("%s: receive 1st large msg err(%d)", __func__, ret);
		kfree(large_msg_data);
		return ret;
	}

	/* empty the previous context data */
	if (data->read_context_length != 0)
		kfree(data->context_data);

	/* large_msg_data[0] of the first msg: total number of large msg
	 * large_msg_data[1-2] of the first msg: total msg length
	 * large_msg_data[3-N] of the first msg: the first msg data itself */
	total_msg_number = large_msg_data[0];
	data->read_context_length = (int)((unsigned int)large_msg_data[1] << 8)
				+ (unsigned int)large_msg_data[2];
	data->context_data = kzalloc((data->read_context_length * sizeof(char)),
						GFP_KERNEL);

	/* copy the fist msg data into context_data */
	memcpy(data->context_data, &large_msg_data[3],
		(length - 3) * sizeof(char));
	kfree(large_msg_data);

	context_data_locater = length - 3;

	/* 2nd, 3rd,...Nth msg */
	for (msg_number = 0; msg_number < total_msg_number; msg_number++) {
		/* receive Nth msg length */
		send_data[0] = MSG2SSP_STT;
		send_data[1] = 0x81 + msg_number;

		/* receive_data(msg length) is two byte because msg is large */
		ret = ssp_i2c_transfer(data, send_data, 2, receive_data, 2);
		if (ret < 0) {
			pr_err("%s: MSG2SSP_STT i2c err(%d)",
					__func__, ret);
			return ret;
		}

		/* get the Nth msg length */
		length = ((unsigned int)receive_data[0] << 8)
			+ (unsigned int)receive_data[1];
		if (length <= 0) {
			pr_err("%s: %dth large msg data not ready(length=%d)",
					__func__, msg_number + 2, length);
			return -EINVAL;
		}

		large_msg_data = kzalloc((length  * sizeof(char)),
					GFP_KERNEL);

		/* receive Nth msg data */
		send_data[0] = MSG2SSP_SRM;
		ret = ssp_i2c_transfer(data, send_data, 1,
					large_msg_data, length);
		if (ret < 0) {
			pr_err("%s: recieve %dth large msg err(%d)",
			       __func__, msg_number + 2, ret);
			kfree(large_msg_data);
			return ret;
		}

		/* copy(append) Nth msg data into context_data */
		memcpy(&data->context_data[context_data_locater],
			large_msg_data,	length * sizeof(char));
		context_data_locater += length;
		kfree(large_msg_data);
	}

	return data->read_context_length;
}

static long ssp_sensorhub_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct ssp_data *data = container_of(file->private_data,
				     struct ssp_data, sensorhub_device);
	char *context_data;
	int length = 0;
	int ret = 0;

	switch (cmd) {
	case IOCTL_WRITE_CONTEXT_SIZE:
		ret = copy_from_user(&length, argp, _IOC_SIZE(cmd));
		if (ret < 0) {
			pr_err("%s: receive context length from user err(%d)",
				__func__, ret);
			goto exit;
		} else if (length <= 0) {
			pr_err("%s: invalid context write length(%d)",
				__func__, length);
			ret = -EINVAL;
			goto exit;
		}

		data->write_context_length = length;
		break;

	case IOCTL_WRITE_CONTEXT_DATA:
		length = data->write_context_length;
		if (length <= 0) {
			pr_err("%s: invalid context write length(%d)",
				__func__, length);
			ret = -EINVAL;
			goto exit;
		}

		context_data = kzalloc(length * sizeof(char), GFP_KERNEL);
		if (!context_data) {
			pr_err("%s: alloc memory for context data err",
				__func__);
			ret = -ENOMEM;
			goto exit;
		}

		ret = copy_from_user(context_data, argp, length);
		if (ret < 0) {
			pr_err("%s: receive context data from user err(%d)",
				__func__, ret);
			kfree(context_data);
			goto exit;
		}

		ret = ssp_send_data(data, context_data, length);
		if (ret < 0) {
			pr_err("%s: write context data into MCU err(%d)",
				__func__, ret);
			kfree(context_data);
			goto exit;
		}

		kfree(context_data);
		data->write_context_length = 0;
		break;

	case IOCTL_READ_CONTEXT_DATA:
		if (data->read_context_length <= 0) {
			pr_err("%s: invalid context read length(%d)",
				__func__, data->read_context_length);
			ret = -EINVAL;
			goto exit;
		} else if (!data->context_data) {
			pr_err("%s: no context data", __func__);
			ret = -ENODEV;
			goto exit;
		}

		ret = copy_to_user(argp, data->context_data,
				data->read_context_length);
		if (ret < 0) {
			pr_err("%s: send context data to user err(%d)",
				__func__, ret);
			goto exit;
		}

		kfree(data->context_data);
		data->read_context_length = 0;
		break;

	default:
		pr_err("%s: icotl cmd err(%d)", __func__, cmd);
		ret = -EINVAL;
	}

exit:
	return ret;
}

static const struct file_operations ssp_sensorhub_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.unlocked_ioctl = ssp_sensorhub_ioctl,
};

static int ssp_get_context_length(struct ssp_data *data,
			       int total_length, int current_length)
{
	if (total_length < current_length) {
		pr_err("%s: total_length < current_length", __func__);
		return -EINVAL;
	}

	if (data->read_context_length != 0)
		kfree(data->context_data);

	data->read_context_length = total_length - current_length;
	data->context_data = kzalloc((data->read_context_length * sizeof(char)),
				      GFP_KERNEL);

	return 0;
}

static int ssp_get_context_data(struct ssp_data *data,
			     char *dataframe, int total_length)
{
	int current_index = total_length - data->read_context_length;
	int temp = 0;

	if (current_index < 0) {
		pr_err("%s: current_index < 0", __func__);
		return -EINVAL;
	}

	pr_err("%s: library length = %d", __func__,
		data->read_context_length);

	while (current_index < total_length) {
		data->context_data[temp++] = dataframe[current_index++];
		pr_err("%s: library data[%d] = %d",
			__func__, temp-1, data->context_data[temp-1]);
	}

	return 0;
}

void ssp_report_context_data(struct ssp_data *data)
{
	input_report_rel(data->context_input_dev, REL_RX,
			data->read_context_length);
	input_sync(data->context_input_dev);
}

void ssp_report_context_noti(struct ssp_data *data, char noti)
{
	input_report_rel(data->context_input_dev, REL_RY, noti);
	input_sync(data->context_input_dev);

	if (noti == MSG2SSP_AP_STATUS_WAKEUP)
		pr_err("%s: wake up", __func__);
	else if (noti == MSG2SSP_AP_STATUS_SLEEP)
		pr_err("%s: sleep", __func__);
}

int ssp_handle_sensorhub_data(struct ssp_data *data, char *dataframe,
				int *data_index, int length)
{
	int ret;

	ret = ssp_get_context_length(data, length, (*data_index)++);
	if (ret < 0) {
		pr_err("%s: get context length err(%d)", __func__, ret);
		goto exit;
	}

	ret = ssp_get_context_data(data, dataframe, length);
	if (ret < 0) {
		pr_err("%s: get context data err(%d)", __func__, ret);
		goto exit;
	}

	ssp_report_context_data(data);

exit:
	(*data_index)++;
	return ret;
}

int ssp_initialize_sensorhub(struct ssp_data *data)
{
	int ret;

	/* allocate sensor hub context input devices */
	data->context_input_dev = input_allocate_device();
	if (!data->context_input_dev) {
		pr_err("%s: allocate context input devices err", __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_context;
	}

	wake_lock_init(&data->sensorhub_wake_lock, WAKE_LOCK_SUSPEND,
			"sensorhub_wake_lock");

	ret = input_register_device(data->context_input_dev);
	if (ret < 0) {
		pr_err("%s: could not register context input device(%d)",
			__func__, ret);
		input_free_device(data->context_input_dev);
		goto err_input_register_device_context;
	}

	input_set_drvdata(data->context_input_dev, data);
	data->context_input_dev->name = "ssp_context";

	input_set_capability(data->context_input_dev, EV_REL, REL_RX);
	input_set_abs_params(data->context_input_dev, REL_RX,
				0, 65535, 0, 0);

	input_set_capability(data->context_input_dev, EV_REL, REL_RY);
	input_set_abs_params(data->context_input_dev, REL_RY,
				0, 65535, 0, 0);

	/* create sensorhub device node */
	data->sensorhub_device.minor = MISC_DYNAMIC_MINOR;
	data->sensorhub_device.name = "ssp_sensorhub";
	data->sensorhub_device.fops = &ssp_sensorhub_fops;

	ret = misc_register(&data->sensorhub_device);
	if (ret < 0) {
		pr_err("%s: misc_register() failed", __func__);
		goto err_misc_register;
	}

	return 0;

err_misc_register:
	input_unregister_device(data->context_input_dev);
err_input_register_device_context:
	wake_lock_destroy(&data->sensorhub_wake_lock);
err_input_allocate_device_context:
	return ret;
}

void ssp_remove_sensorhub(struct ssp_data *data)
{
	misc_deregister(&data->sensorhub_device);
	input_unregister_device(data->context_input_dev);
	wake_lock_destroy(&data->sensorhub_wake_lock);
}

MODULE_DESCRIPTION("Samsung Sensor Platform(SSP) sensorhub driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
