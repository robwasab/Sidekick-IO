/*
 * rjt_usb_bridge_gpio.c
 *
 * Created: 11/26/2020 2:20:52 PM
 *  Author: robbytong
 */ 

#include "rjt_external_interrupt_controller.h"
#include "rjt_usb_bridge_app.h"
#include "rjt_logger.h"
#include "utils.h"

#include <port.h>
#include <stdbool.h>
#include <asf.h>

static uint32_t mPinInterruptStatus = 0;
static RJTEIC_t mEICModule;


static void set_pin_interrupt_flag(uint8_t logical_gpio_no)
{
	ASSERT(logical_gpio_no < 32);

	system_interrupt_enter_critical_section();
	mPinInterruptStatus |= (1 << logical_gpio_no);
	//RJTLogger_print("logical gpio: %d", logical_gpio_no);
	system_interrupt_leave_critical_section();
}


static void ext_interrupt_callback(void * self, uint8_t pinno, uint8_t intno)
{
	//RJTLogger_print("EIC pinno: %d extintno: %d triggered", pinno, intno);

	if(pinno == PIN_PA15) 
	{
		// switch pin on xplained board
		set_pin_interrupt_flag(31);
		RJTUSBBridge_setInterruptBit(RJT_USB_INTERRUPT_BIT_GPIO, true);
	} 
	else {
		bool success = false;
		uint8_t index = 0xff;
		RJTUSBBridgeConfig_gpio2index(pinno, &success, &index);

		if(true == success) 
		{
			ASSERT(0xff != index);
			set_pin_interrupt_flag(index);
			RJTUSBBridge_setInterruptBit(RJT_USB_INTERRUPT_BIT_GPIO, true);
		}
		else {
			RJTLogger_print("pin interrupt triggered on unknown pin: %d", pinno);
		}
	}
}


#define GPIO_VERIFY_CMD_INDEX_BEGIN \
do { \
	bool success = false; \
	uint8_t gpio = 0xff; \
	uint8_t extint = 0xff; \
	RJTUSBBridgeConfig_index2gpio(cmd.index, &success, &gpio); \
	if(true == success) { \
		success = false; \
		RJTUSBBridgeConfig_index2extint(cmd.index, &success, &extint); \
		if(true == success) {


#define GPIO_VERIFY_CMD_INDEX_END \
		} else { \
			RJTLogger_print("GPIO: index2extint failed..."); \
			*rsp_len = 0; \
			return RJT_USB_ERROR_PARAMETER; \
		} \
	} else { \
		/* index could not be converted to gpio */ \
		RJTLogger_print("GPIO: index2gpio failed..."); \
		*rsp_len = 0; \
		return RJT_USB_ERROR_PARAMETER; \
	} \
} while(0); \
return RJT_USB_ERROR_NONE;


enum RJT_USB_ERROR RJTUSBBridgeGPIO_pinRead(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * rsp_data, size_t * rsp_len)
{
	//RJTLogger_print("GPIO: pinRead()");
	
	// This function returns a byte in the response, make sure we have the space to do so
	ASSERT(*rsp_len >= 1);

	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t index;
	RJT_USB_BRIDGE_END_CMD


	GPIO_VERIFY_CMD_INDEX_BEGIN
	{
		bool level = port_pin_get_input_level(gpio);
		
		rsp_data[0] = (uint8_t) level;
		*rsp_len = 1;
	}
	GPIO_VERIFY_CMD_INDEX_END
};


enum RJT_USB_ERROR RJTUSBBridgeGPIO_pinSet(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * rsp_data, size_t * rsp_len)
{
	//sRJTLogger_print("GPIO: pinSet()");

	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t index;
		uint8_t val;
	RJT_USB_BRIDGE_END_CMD
	
	
	GPIO_VERIFY_CMD_INDEX_BEGIN
	{
		//RJTLogger_print("GPIO: setting pin...");
		port_pin_set_output_level(gpio, !!cmd.val);
		*rsp_len = 0;
		//RJTLogger_print("GPIO: Set index %d gpio %x: %?", cmd.index, gpio, cmd.val);
	}
	GPIO_VERIFY_CMD_INDEX_END	
};


enum RJT_USB_ERROR RJTUSBBridgeGPIO_enablePinInterrupt(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	//RJTLogger_print("GPIO: enablePinInterrupt()");

	RJT_USB_BRIDGE_BEGIN_CMD
	uint8_t index;
	uint8_t detection;
	RJT_USB_BRIDGE_END_CMD

	GPIO_VERIFY_CMD_INDEX_BEGIN
	{
		enum RJT_EIC_DETECTION cmd2detection[] = {
			[0] = RJT_EIC_DETECTION_FALL,
			[1] = RJT_EIC_DETECTION_RISE,
			[2] = RJT_EIC_DETECTION_BOTH,
		};
		
		if(cmd.detection < ARRAY_SIZE(cmd2detection))
		{
			RJTEICConfig_t config = {
				.ext_int_sel = extint,
				.eic_detection = cmd2detection[cmd.detection],
				.gpio = gpio,
				.gpio_mux_position = 0,
				.callback = ext_interrupt_callback,
			};

			RJTEIC_configure(&mEICModule, &config);

			RJTEIC_enableInterrupt(extint);
				
			*rsp_len = 0;
		}
		else {
			*rsp_len = 0;
			return RJT_USB_ERROR_PARAMETER;
		}
	}
	GPIO_VERIFY_CMD_INDEX_END
}


enum RJT_USB_ERROR RJTUSBBridgeGPIO_disablePinInterrupt(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	//RJTLogger_print("GPIO: disablePinInterrupt()");

	RJT_USB_BRIDGE_BEGIN_CMD
	uint8_t index;
	RJT_USB_BRIDGE_END_CMD


	GPIO_VERIFY_CMD_INDEX_BEGIN
	{
		RJTEIC_disableInterrupt(extint);
		
		*rsp_len = 0;
	}
	GPIO_VERIFY_CMD_INDEX_END
}


enum RJT_USB_ERROR RJTUSBBridgeGPIO_getInterruptStatus(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	//RJTLogger_print("GPIO: getInterruptStatus()");

	ASSERT(*rsp_len >= sizeof(mPinInterruptStatus));

	memcpy(rsp_data, &mPinInterruptStatus, sizeof(mPinInterruptStatus));

	*rsp_len = sizeof(mPinInterruptStatus);

	return RJT_USB_ERROR_NONE;
}


enum RJT_USB_ERROR RJTUSBBridgeGPIO_clearInterruptStatus(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	RJT_USB_BRIDGE_BEGIN_CMD
	uint32_t mask;
	RJT_USB_BRIDGE_END_CMD

	system_interrupt_enter_critical_section();
	mPinInterruptStatus &= ~cmd.mask;

	if(0 == mPinInterruptStatus) {
		// Clear the global interrupt bit
		RJTUSBBridge_clearInterruptBit(RJT_USB_INTERRUPT_BIT_GPIO, true);
	}
	system_interrupt_leave_critical_section();

	*rsp_len = 0;

	return RJT_USB_ERROR_NONE;
}

enum RJT_USB_ERROR RJTUSBBridgeGPIO_parallelWrite(
	const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	RJT_USB_BRIDGE_BEGIN_CMD
		uint32_t index_mask;
	RJT_USB_BRIDGE_END_CMD

	RJTLogger_print("GPIO: parallelWrite(): %b", cmd.index_mask);

	const uint32_t port_mask = ((1 << RJT_USB_BRIDGE_NUM_GPIOS) - 1);

	cmd.index_mask &= port_mask;

	uint32_t portb = PORTB.OUT.reg;

	// Clear the port mask
	portb &= ~port_mask;

	// Mask in the desired bits
	portb |= port_mask;

	// Write back to register
	PORTB.OUT.reg = portb;

	return RJT_USB_ERROR_NONE;
}


enum RJT_USB_ERROR RJTUSBBridgeGPIO_configureIndex(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	RJTLogger_print("GPIO: configureIndex()");

	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t index;
		uint8_t dir;
		uint8_t pull;
	RJT_USB_BRIDGE_END_CMD

	// Check to see if GPIO pin number is valid
	bool success = false;
	uint8_t gpio = 0xff;

	RJTUSBBridgeConfig_index2gpio(cmd.index, &success, &gpio);

	if(true == success && 0xff != gpio)
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
				
				*rsp_len = 0;
				return RJT_USB_ERROR_NONE;
			}
			else {
				*rsp_len = 0;
				return RJT_USB_ERROR_PARAMETER;
			}
		}
		else {
			*rsp_len = 0;
			return RJT_USB_ERROR_PARAMETER;
		}
	}
	else {
		RJTLogger_print("gpio pin not available");
		*rsp_len = 0;
		return RJT_USB_ERROR_PARAMETER;
	}
};


enum RJT_USB_ERROR RJTUSBBridgeGPIO_setLed(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	//RJTLogger_print("GPIO: setLed()");

	RJT_USB_BRIDGE_BEGIN_CMD
	uint8_t on;
	RJT_USB_BRIDGE_END_CMD

	port_pin_set_output_level(PIN_PB30, !cmd.on);
	
	*rsp_len = 0;
	return RJT_USB_ERROR_NONE;
}


void RJTUSBBridgeGPIO_init(void)
{
	RJTEIC_init(&mEICModule);

	RJTEICConfig_t config = {
		.ext_int_sel = RJT_EIC_EXT_INT15,
		.eic_detection = RJT_EIC_DETECTION_FALL,
		.gpio = PIN_PA15,
		.gpio_mux_position = MUX_PA15A_EIC_EXTINT15,
		.callback = ext_interrupt_callback,
	};

	RJTEIC_configure(&mEICModule, &config);

	RJTEIC_enableInterrupt(RJT_EIC_EXT_INT15);
}