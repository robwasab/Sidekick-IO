/*
 * rjt_usb_bridge_gpio.c
 *
 * Created: 11/26/2020 2:20:52 PM
 *  Author: robbytong
 */ 

#include "rjt_usb_bridge.h"
#include "rjt_logger.h"
#include "utils.h"

#include <port.h>

enum RJT_USB_ERROR RJTUSBBridgeGPIO_pinRead(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * rsp_data, size_t * rsp_len)
{
	RJTLogger_print("GPIO: pinRead()");

	*rsp_len = 0;

	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t val;
	RJT_USB_BRIDGE_END_CMD

	return RJT_USB_ERROR_NONE;	
};


enum RJT_USB_ERROR RJTUSBBridgeGPIO_pinSet(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * rsp_data, size_t * rsp_len)
{
	RJTLogger_print("GPIO: pinSet()");

	*rsp_len = 0;
		
	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t index;
		uint8_t val;
	RJT_USB_BRIDGE_END_CMD
	
	// Check to see if GPIO pin number is valid
	bool success = false;
	uint8_t gpio = 0xff;

	RJTUSBBridgeConfig_index2gpio(cmd.index, &success, &gpio);

	if(true == success)
	{
		port_pin_set_output_level(gpio, !!cmd.val);

		RJTLogger_print("GPIO: Set gpio %x", gpio);

		return RJT_USB_ERROR_NONE;
	}
	else {
		RJTLogger_print("GPIO: index2gpio failed...");
		return RJT_USB_ERROR_PARAMETER;
	}
};


enum RJT_USB_ERROR RJTUSBBridgeGPIO_configureIndex(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * rsp_data, size_t * rsp_len)
{
	RJTLogger_print("GPIO: configureIndex()");

	*rsp_len = 0;
				
	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t pin;
		uint8_t dir;
		uint8_t pull;
	RJT_USB_BRIDGE_END_CMD


	// Check to see if GPIO pin number is valid
	bool success = false;
	uint8_t gpio = 0xff;

	RJTUSBBridgeConfig_index2gpio(cmd.pin, &success, &gpio);

	if(true == success)
	{
		// Check direction
		const uint8_t dir_map[] = {
			[0] = PORT_PIN_DIR_INPUT,
			[1] = PORT_PIN_DIR_OUTPUT,
		};

		if(cmd.dir < ARRAY_SIZE(dir_map))
		{
			// Check pull configuration
			const uint8_t pull_map[] = {
				[0] = PORT_PIN_PULL_NONE,
				[1] = PORT_PIN_PULL_UP,
				[2] = PORT_PIN_PULL_DOWN,
			};
		
			if(cmd.pull < ARRAY_SIZE(pull_map))
			{
				// All parameters valid

				// Get configuration defaults
				struct port_config config;

				port_get_config_defaults(&config);

				config.direction = dir_map[cmd.dir];
				config.input_pull = pull_map[cmd.pull];
			
				port_pin_set_config(gpio, &config);
			
				RJTLogger_print("GPIO: configure success!");
				
				return RJT_USB_ERROR_NONE;
			}
			else {
				return RJT_USB_ERROR_PARAMETER;
			}
		}
		else {
			return RJT_USB_ERROR_PARAMETER;
		}
	}
	else {
		return RJT_USB_ERROR_PARAMETER;
	}
};