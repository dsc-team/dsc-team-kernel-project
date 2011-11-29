#
# Makefile for miscellaneous I2C chip drivers.
#
# Do not add new drivers to this directory! It is DEPRECATED.
#
# Device drivers are better grouped according to the functionality they
# implement rather than to the bus they are connected to. In particular:
# * Hardware monitoring chip drivers go to drivers/hwmon
# * RTC chip drivers go to drivers/rtc
# * I/O expander drivers go to drivers/gpio
#


obj-$(CONFIG_SENSORS_MT9T013)	+= mt9t013.o

# Jagan+
obj-$(CONFIG_GSENSOR_BOSCH_BMA150)      += gsensor-bosch-bma150.o
obj-$(CONFIG_ECOMPASS_AKM_AK8973S)      += ecompass-akm-ak8973s.o
obj-$(CONFIG_BCOM_FM)           	+= bcom_fm.o
# Jagan-

ifeq ($(CONFIG_I2C_DEBUG_CHIP),y)
EXTRA_CFLAGS += -DDEBUG
endif

