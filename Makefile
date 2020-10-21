PROGRAM = multi_sensor

EXTRA_COMPONENTS = \
	extras/dht \
	extras/i2c \
	extras/pcf8574 \
	extras/hd44780 \
	extras/http-parser \
	extras/dhcpserver \
	$(abspath ../../components/esp8266-open-rtos/wifi_config) \
	$(abspath ../../components/esp8266-open-rtos/cJSON) \
	$(abspath ../../components/common/button) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit) \
	$(abspath ../../components/esp8266-open-rtos/qrcode)


HD44780_I2C = 1

FLASH_SIZE ?= 8
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0xF5000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS \
	-DHOMEKIT_PASSWORD="$(HOMEKIT_PASSWORD)" \
	-DHOMEKIT_SETUP_ID="$(HOMEKIT_SETUP_ID)"

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
