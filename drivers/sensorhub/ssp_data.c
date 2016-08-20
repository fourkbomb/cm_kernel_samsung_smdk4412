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

/* SSP -> AP Instruction */
#define MSG2AP_INST_BYPASS_DATA			0x00
#define MSG2AP_INST_LIBRARY_DATA		0x01
#define MSG2AP_INST_SELFTEST_DATA		0x02

/* Factory data length */
#define ACCEL_FACTORY_DATA_LENGTH		1
#define GYRO_FACTORY_DATA_LENGTH		21
#define MAGNETIC_FACTORY_DATA_LENGTH		6
#define PRESSURE_FACTORY_DATA_LENGTH		1
#define MCU_FACTORY_DATA_LENGTH			5
#define	GYRO_TEMP_FACTORY_DATA_LENGTH		1
#define	GYRO_DPS_FACTORY_DATA_LENGTH		1

/*************************************************************************/
/* AKM Daemon Library ioctl						 */
/*************************************************************************/

static int akmd_copy_in(unsigned int cmd, void __user *argp,
			void *buf, size_t buf_size)
{
	if (!(cmd & IOC_IN))
		return 0;
	if (_IOC_SIZE(cmd) > buf_size)
		return -EINVAL;
	if (copy_from_user(buf, argp, _IOC_SIZE(cmd)))
		return -EFAULT;
	return 0;
}

static int akmd_copy_out(unsigned int cmd, void __user *argp,
			 void *buf, size_t buf_size)
{
	if (!(cmd & IOC_OUT))
		return 0;
	if (_IOC_SIZE(cmd) > buf_size)
		return -EINVAL;
	if (copy_to_user(argp, buf, _IOC_SIZE(cmd)))
		return -EFAULT;
	return 0;
}

long akmd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int iRet;
	void __user *argp = (void __user *)arg;
	struct ssp_data *data = container_of(file->private_data,
					struct ssp_data, akmd_device);

	union {
		u8 uData[8];
		u8 uMagData[8];
		u8 uFuseData[3];
		int iAccData[3];
	} akmdbuf;

	iRet = akmd_copy_in(cmd, argp, akmdbuf.uData, sizeof(akmdbuf));
	if (iRet)
		return iRet;

	switch (cmd) {
	case ECS_IOCTL_GET_MAGDATA:
		akmdbuf.uMagData[0] = 1;
		akmdbuf.uMagData[1] = data->buf[GEOMAGNETIC_SENSOR].x & 0xff;
		akmdbuf.uMagData[2] = data->buf[GEOMAGNETIC_SENSOR].x >> 8;
		akmdbuf.uMagData[3] = data->buf[GEOMAGNETIC_SENSOR].y & 0xff;
		akmdbuf.uMagData[4] = data->buf[GEOMAGNETIC_SENSOR].y >> 8;
		akmdbuf.uMagData[5] = data->buf[GEOMAGNETIC_SENSOR].z & 0xff;
		akmdbuf.uMagData[6] = data->buf[GEOMAGNETIC_SENSOR].z >> 8;
		akmdbuf.uMagData[7] = 0x10;
		break;
	case ECS_IOCTL_GET_ACCDATA:
		akmdbuf.iAccData[0] = data->buf[ACCELEROMETER_SENSOR].x;
		akmdbuf.iAccData[1] = data->buf[ACCELEROMETER_SENSOR].y;
		akmdbuf.iAccData[2] = data->buf[ACCELEROMETER_SENSOR].z;
		break;
	case ECS_IOCTL_GET_FUSEROMDATA:
		akmdbuf.uFuseData[0] = data->uFuseRomData[0];
		akmdbuf.uFuseData[1] = data->uFuseRomData[1];
		akmdbuf.uFuseData[2] = data->uFuseRomData[2];
		break;
	default:
		return -ENOTTY;
	}

	if (iRet < 0)
		return iRet;

	return akmd_copy_out(cmd, argp, akmdbuf.uData, sizeof(akmdbuf));
}

/*************************************************************************/
/* SSP parsing the dataframe                                             */
/*************************************************************************/

static void get_3axis_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	int iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->x = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->y = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->z = iTemp;

	data_dbg("x: %d, y: %d, z: %d\n", sensorsdata->x,
		sensorsdata->y, sensorsdata->z);
}

static void get_light_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	int iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->r = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->g = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->b = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->w = iTemp;

	data_dbg("r: %d, g: %d, b: %d, w: %d\n", sensorsdata->r,
		sensorsdata->g, sensorsdata->b, sensorsdata->w);
}

static void get_pressure_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	int iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 16;
	sensorsdata->pressure[0] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	sensorsdata->pressure[0] += iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->pressure[0] += iTemp;


	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	sensorsdata->pressure[1] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->pressure[1] += iTemp;

	data_dbg("p : %u, t: %u\n", sensorsdata->pressure[0],
		sensorsdata->pressure[1]);
}

static void get_gesture_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	int iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->data[0] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->data[1] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->data[2] = iTemp;

	iTemp = (int)pchRcvDataFrame[(*iDataIdx)++];
	iTemp <<= 8;
	iTemp += pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->data[3] = iTemp;

	data_dbg("A: %d, B: %d, C: %d, D: %d\n",
		sensorsdata->data[0], sensorsdata->data[1],
		sensorsdata->data[2], sensorsdata->data[3]);
}

static void get_proximity_sensordata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	sensorsdata->prox[0] = (u8)pchRcvDataFrame[(*iDataIdx)++];
	sensorsdata->prox[1] = (u8)pchRcvDataFrame[(*iDataIdx)++];

	data_dbg("prox : %u, %u\n", sensorsdata->prox[0], sensorsdata->prox[1]);
}

static void get_proximity_rawdata(char *pchRcvDataFrame, int *iDataIdx,
	struct sensor_value *sensorsdata)
{
	sensorsdata->prox[0] = (u8)pchRcvDataFrame[(*iDataIdx)++];

	data_dbg("proxraw : %u\n", sensorsdata->prox[0]);
}

static void get_factoty_data(struct ssp_data *data, int iSensorData,
	char *pchRcvDataFrame, int *iDataIdx)
{
	int iIdx, iTotalLenth = 0;

	data->uFactorydataReady = 0;

	if (iSensorData == ACCELEROMETER_FACTORY) {
		data->uFactorydataReady = (1 << ACCELEROMETER_FACTORY);
		iTotalLenth = ACCEL_FACTORY_DATA_LENGTH;
	} else if (iSensorData == GYROSCOPE_FACTORY) {
		data->uFactorydataReady = (1 << GYROSCOPE_FACTORY);
		iTotalLenth = GYRO_FACTORY_DATA_LENGTH;
	} else if (iSensorData == GEOMAGNETIC_FACTORY) {
		data->uFactorydataReady = (1 << GEOMAGNETIC_FACTORY);
		iTotalLenth = MAGNETIC_FACTORY_DATA_LENGTH;
	} else if (iSensorData == PRESSURE_FACTORY) {
		data->uFactorydataReady = (1 << PRESSURE_FACTORY);
		iTotalLenth = PRESSURE_FACTORY_DATA_LENGTH;
	} else if (iSensorData == MCU_FACTORY) {
		data->uFactorydataReady = (1 << MCU_FACTORY);
		iTotalLenth = MCU_FACTORY_DATA_LENGTH;
		ssp_dbg("[SSP]: %s - Mcu test data\n", __func__);
	} else if (iSensorData == GYROSCOPE_TEMP_FACTORY) {
		data->uFactorydataReady = (1 << GYROSCOPE_TEMP_FACTORY);
		iTotalLenth = GYRO_TEMP_FACTORY_DATA_LENGTH;
	} else if (iSensorData == GYROSCOPE_DPS_FACTORY) {
		data->uFactorydataReady = (1 << GYROSCOPE_DPS_FACTORY);
		iTotalLenth = GYRO_DPS_FACTORY_DATA_LENGTH;
	}

	for (iIdx = 0; iIdx < iTotalLenth; iIdx++)
		data->uFactorydata[iIdx] = (u8)pchRcvDataFrame[(*iDataIdx)++];
}

int parse_dataframe(struct ssp_data *data, char *pchRcvDataFrame, int iLength)
{
	int iDataIdx, iSensorData;
	struct sensor_value *sensorsdata;

	sensorsdata = kzalloc(sizeof(*sensorsdata), GFP_KERNEL);
	if (sensorsdata == NULL)
		return ERROR;

	for (iDataIdx = 0; iDataIdx < iLength;) {
		if (pchRcvDataFrame[iDataIdx] == MSG2AP_INST_BYPASS_DATA) {
			iDataIdx++;
			iSensorData = pchRcvDataFrame[iDataIdx++];
			if ((iSensorData < 0) ||
				(iSensorData >= (SENSOR_MAX - 1))) {
				pr_err("[SSP]: %s - Mcu data frame1 error %d\n",
					__func__, iSensorData);
				kfree(sensorsdata);
				return ERROR;
			}

			data->get_sensor_data[iSensorData](pchRcvDataFrame,
				&iDataIdx, sensorsdata);
			data->report_sensor_data[iSensorData](data,
				sensorsdata);

		} else if (pchRcvDataFrame[iDataIdx] ==
			MSG2AP_INST_SELFTEST_DATA) {
			iDataIdx++;
			iSensorData = pchRcvDataFrame[iDataIdx++];
			if ((iSensorData < 0) ||
				(iSensorData >= SENSOR_FACTORY_MAX)) {
				pr_err("[SSP]: %s - Mcu data frame2 error %d\n",
					__func__, iSensorData);
				kfree(sensorsdata);
				return ERROR;
			}
			get_factoty_data(data, iSensorData, pchRcvDataFrame,
				&iDataIdx);
#ifdef CONFIG_SENSORS_SSP_SENSORHUB
		} else if (pchRcvDataFrame[iDataIdx] ==
			MSG2AP_INST_LIBRARY_DATA) {
			if (data->library_inst_cnt++ == 0) {
				int ret;
				ret = ssp_handle_sensorhub_data(data,
					pchRcvDataFrame, &iDataIdx, iLength);
				if (ret < 0)
					pr_err("%s: handle sensorhub "
						"data(%d) err(%d)",
						__func__, iDataIdx, ret);
			} else {
				iDataIdx++;
			}
#endif
		} else
			iDataIdx++;
	}
#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	data->library_inst_cnt = 0;
#endif
	kfree(sensorsdata);
	return SUCCESS;
}

void initialize_function_pointer(struct ssp_data *data)
{
	data->get_sensor_data[ACCELEROMETER_SENSOR] = get_3axis_sensordata;
	data->get_sensor_data[GYROSCOPE_SENSOR] = get_3axis_sensordata;
	data->get_sensor_data[GEOMAGNETIC_SENSOR] = get_3axis_sensordata;
	data->get_sensor_data[PRESSURE_SENSOR] = get_pressure_sensordata;
	data->get_sensor_data[GESTURE_SENSOR] = get_gesture_sensordata;
	data->get_sensor_data[PROXIMITY_SENSOR] = get_proximity_sensordata;
	data->get_sensor_data[PROXIMITY_RAW] = get_proximity_rawdata;
	data->get_sensor_data[LIGHT_SENSOR] = get_light_sensordata;

	data->report_sensor_data[ACCELEROMETER_SENSOR] = report_acc_data;
	data->report_sensor_data[GYROSCOPE_SENSOR] = report_gyro_data;
	data->report_sensor_data[GEOMAGNETIC_SENSOR] = report_mag_data;
	data->report_sensor_data[PRESSURE_SENSOR] = report_pressure_data;
	data->report_sensor_data[GESTURE_SENSOR] = report_gesture_data;
	data->report_sensor_data[PROXIMITY_SENSOR] = report_prox_data;
	data->report_sensor_data[PROXIMITY_RAW] = report_prox_raw_data;
	data->report_sensor_data[LIGHT_SENSOR] = report_light_data;
}
