/*
 * rjt_usb_bridge_configuration.c
 *
 * Created: 11/26/2020 3:40:31 PM
 *  Author: robbytong
 */ 

#include "rjt_usb_bridge.h"
#include "rjt_logger.h"
#include "utils.h"

#include <samd21.h>
#include <port.h>

static enum RJT_USB_CONFIG mCurrentConfig = RJT_USB_CONFIG_GPIO;


static uint8_t mConfig2GPIOs[RJT_USB_CONFIG_MAX][RJT_USB_BRIDGE_NUM_GPIOS] = {
	[RJT_USB_CONFIG_GPIO]       = {PIN_PB00, PIN_PB01, PIN_PB06, PIN_PB07, PIN_PB02, PIN_PB03, PIN_PB04, PIN_PB05},
	[RJT_USB_CONFIG_SPI_MASTER] = {    0xff,     0xff,     0xff,     0xff, PIN_PB02, PIN_PB03, PIN_PB04, PIN_PB05},
	[RJT_USB_CONFIG_SPI_SLAVE]  = {    0xff,     0xff,     0xff,     0xff, PIN_PB02, PIN_PB03, PIN_PB04, PIN_PB05},
};


static void uninit_current_config(void)
{
	switch(mCurrentConfig)
	{
		case RJT_USB_CONFIG_GPIO:
			return;
	
		default:
			ASSERT(false);
			break;
	}
}


static enum RJT_USB_ERROR config_gpio(void)
{
	struct port_config default_config;
	port_get_config_defaults(&default_config);

	default_config.direction  = PORT_PIN_DIR_INPUT;
	default_config.input_pull = PORT_PIN_PULL_NONE;
	default_config.powersave = true;

	uint8_t k;

	for(k = 0; k < RJT_USB_BRIDGE_NUM_GPIOS; k++)
	{
		// Set to input, no pull up or down, disable input buffer.
		port_pin_set_config(mConfig2GPIOs[RJT_USB_CONFIG_GPIO][k], &default_config);
	}

	mCurrentConfig = RJT_USB_CONFIG_GPIO;

	RJTLogger_print("CONFIG: gpio");
	
	return RJT_USB_ERROR_NONE;
}


#if 0
static enum RJT_USB_ERROR config_spi_master(void)
{
	uninit_current_config();

	config_gpio();

	return RJT_USB_ERROR_NONE;
}
#endif

void RJTUSBBridgeConfig_index2gpio(uint8_t index, bool * success, uint8_t * gpio)
{
	if(index < RJT_USB_BRIDGE_NUM_GPIOS) 
	{
		uint8_t gpio_num = mConfig2GPIOs[mCurrentConfig][index];

		if(0xff != gpio_num) 
		{
			*gpio = mConfig2GPIOs[mCurrentConfig][index];
			*success = true;
		}
		else {
			// gpio number is in use by another function
			*success = false;
		}
	}
	else {
		// index exceeds available gpios
		*success = false;
	}
}


enum RJT_USB_ERROR RJTUSBBridgeConfig_setConfig(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * rsp_data, size_t * rsp_len)
{
	*rsp_len = 0;

	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t config;
	RJT_USB_BRIDGE_END_CMD


	uninit_current_config();

	switch(cmd.config)
	{
		case RJT_USB_CONFIG_GPIO:
			return config_gpio();
		
		default:
			return RJT_USB_ERROR_PARAMETER;
	}
}


void RJTUSBBridge_configInit(void)
{
	config_gpio();
}
