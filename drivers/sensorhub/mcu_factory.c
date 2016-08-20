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

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define MODEL_NAME			"AT32UC3L0128"

ssize_t mcu_revision_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u,%u\n", get_module_rev(), data->uCurFirmRev);
}

ssize_t mcu_model_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", MODEL_NAME);
}

ssize_t mcu_factorytest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	char chTempBuf[2] = {0, 10};
	int iRet = 0;

	if (sysfs_streq(buf, "1")) {
		data->bMcuIRQTestSuccessed = false;
		data->uFactorydataReady = 0;
		data->uTimeOutCnt = 0;

		iRet = send_instruction(data, FACTORY_MODE,
				MCU_FACTORY, chTempBuf, 2);
		if (data->uTimeOutCnt == 0)
			data->bMcuIRQTestSuccessed = true;
	} else {
		pr_err("[SSP]: %s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	ssp_dbg("[SSP]: MCU Factory Test Start! - %d\n", iRet);

	return size;
}

ssize_t mcu_factorytest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	bool bMcuTestSuccessed = false;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->uFactorydataReady & (1 << MCU_FACTORY)) {
		ssp_dbg("[SSP] MCU Factory Test Data : %u, %u, %u, %u, %u\n",
			data->uFactorydata[0], data->uFactorydata[1],
			data->uFactorydata[2], data->uFactorydata[3],
			data->uFactorydata[4]);

		/* system clock, RTC, I2C Master, I2C Slave, externel pin */
		if ((data->uFactorydata[0] == SUCCESS)
			&& (data->uFactorydata[1] == SUCCESS)
			&& (data->uFactorydata[2] == SUCCESS)
			&& (data->uFactorydata[3] == SUCCESS)
			&& (data->uFactorydata[4] == SUCCESS))
			bMcuTestSuccessed = true;
	}

	ssp_dbg("[SSP]: MCU Factory Test Result - %s, %s, %s\n", MODEL_NAME,
		(data->bMcuIRQTestSuccessed ? "OK" : "NG"),
		(bMcuTestSuccessed ? "OK" : "NG"));

	return sprintf(buf, "%s,%s,%s\n", MODEL_NAME,
		(data->bMcuIRQTestSuccessed ? "OK" : "NG"),
		(bMcuTestSuccessed ? "OK" : "NG"));
}
