#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_SENSORS_SYSFS)		+= sensors_core.o accel_factory.o gyro_factory.o proximity_factory.o\
						light_factory.o pressure_factory.o magnetic_factory.o mcu_factory.o
obj-$(CONFIG_SENSORS_SSP)		+= ssp_dev.o ssp_i2c.o ssp_data.o ssp_sysfs.o ssp_input.o ssp_firmware.o
obj-$(CONFIG_SENSORS_SSP_SENSORHUB)	+= ssp_sensorhub.o
