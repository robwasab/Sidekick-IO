/*
 * rjt_usb_bridge_configuration.c
 *
 * Created: 11/26/2020 3:40:31 PM
 *  Author: robbytong
 */ 
#include "rjt_external_interrupt_controller.h"
#include "rjt_usb_bridge.h"
#include "rjt_logger.h"
#include "utils.h"

#include <samd21.h>
#include <port.h>
#include <spi.h>

static enum RJT_USB_CONFIG __mCurrentConfig = RJT_USB_CONFIG_GPIO;


static struct {
	struct spi_module  instance;
	             bool  enabled;
} mSpi;


/**
 * Combination:          - [D]
 *
 * PB00 - SERCOM5/PAD[2] - SS
 * PB01 - SERCOM5/PAD[3] - MISO 
 * PB02 - SERCOM5/PAD[0] - MOSI
 * PB03 - SERCOM5/PAD[1] - SCK
 * PB04
 * PB05
 * PB06
 * PB07
 */
static uint8_t mIndex2Pin[RJT_USB_BRIDGE_NUM_GPIOS] = {
	PIN_PB00, PIN_PB01, PIN_PB02, PIN_PB03, PIN_PB04, PIN_PB05, PIN_PB06, PIN_PB07,
};


static uint8_t mConfig2Override[RJT_USB_CONFIG_MAX][RJT_USB_BRIDGE_NUM_GPIOS] = {
	[RJT_USB_CONFIG_GPIO]       = {0},	// No override necessary
	[RJT_USB_CONFIG_SPI_MASTER] = {
		[0] = 0xff,	// SS
		[1] = 0xff,	// MISO
		[2] = 0xff,	// MOSI
		[3] = 0xff, // SCK
	},
};


static uint8_t pin2extint[] = {
	RJT_EIC_EXT_INT0, RJT_EIC_EXT_INT1, RJT_EIC_EXT_INT2, RJT_EIC_EXT_INT3,
	RJT_EIC_EXT_INT4, RJT_EIC_EXT_INT5, RJT_EIC_EXT_INT6, RJT_EIC_EXT_INT7, 
};


static enum RJT_USB_CONFIG read_current_config(void)
{
	return __mCurrentConfig;
};


static uint8_t config2gpio(enum RJT_USB_CONFIG config, uint8_t index)
{
	ASSERT(index < RJT_USB_BRIDGE_NUM_GPIOS);

	// Check to see if the index is not overridden 
	if(0 == mConfig2Override[config][index]) {
		// Pin is not overridden, return gpio
		return mIndex2Pin[index];
	}
	else {
		// This pin is in use
		return 0xff;
	}
}


void RJTUSBBridgeConfig_gpio2index(uint8_t gpio, bool * success, uint8_t * index)
{
	uint8_t k;

	enum RJT_USB_CONFIG current_config = read_current_config();

	for(k = 0; k < RJT_USB_BRIDGE_NUM_GPIOS; ++k)
	{
		if(gpio == config2gpio(current_config, k)) {
			break;
		}
	}

	if(k == RJT_USB_BRIDGE_NUM_GPIOS) 
	{
		*success = false;
	} else {
		*success = true;
		*index = k;
	}
}


void RJTUSBBridgeConfig_index2extint(uint8_t index, bool * success, uint8_t * extint)
{
	ASSERT(sizeof(pin2extint) == RJT_USB_BRIDGE_NUM_GPIOS);

	if(index < RJT_USB_BRIDGE_NUM_GPIOS)
	{
		*extint = pin2extint[index];
		*success = true;
	}
	else {
		// index exceeds available gpios
		*success = false;
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
		port_pin_set_config(config2gpio(RJT_USB_CONFIG_GPIO, k), &default_config);

		uint8_t extint = 0xff;
		bool success = false;

		RJTUSBBridgeConfig_index2extint(k, &success, &extint);
		ASSERT(true == success);

		RJTEIC_disableInterrupt(extint);
	}

	__mCurrentConfig = RJT_USB_CONFIG_GPIO;

	RJTLogger_print("CONFIG: gpio");
	
	return RJT_USB_ERROR_NONE;
}


struct spi_module * RJTUSBBridgeConfig_getSpiModule(void)
{
	enum RJT_USB_CONFIG current_config = read_current_config();

	if(current_config == RJT_USB_CONFIG_SPI_MASTER || 
	   current_config == RJT_USB_CONFIG_SPI_SLAVE)
	{
		return &mSpi.instance;
	}
	else {
		return NULL;
	}
}


static enum RJT_USB_ERROR config_spi_master(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * rsp_data, size_t * rsp_len)
{
	RJT_USB_BRIDGE_BEGIN_CMD
	uint8_t spi_mode;
	uint8_t data_order;
	RJT_USB_BRIDGE_END_CMD

	ASSERT(false == mSpi.enabled);
	struct spi_config	config;

	spi_get_config_defaults(&config);

	config.character_size = SPI_CHARACTER_SIZE_8BIT;
	config.data_order			= SPI_DATA_ORDER_MSB;
	config.generator_source = GCLK_GENERATOR_0;
	config.master_slave_select_enable = true;
	config.mode = SPI_MODE_MASTER;
	config.mode_specific.master.baudrate = 1000000;

	config.mux_setting = SPI_SIGNAL_MUX_SETTING_D;
	
	config.pinmux_pad0 = PINMUX_PB02D_SERCOM5_PAD0; // MOSI
	config.pinmux_pad1 = PINMUX_PB03D_SERCOM5_PAD1;	// SCK
	config.pinmux_pad2 = PINMUX_PB00D_SERCOM5_PAD2; // SS
	config.pinmux_pad3 = PINMUX_PB01D_SERCOM5_PAD3;	// MISO

	spi_init(&mSpi.instance, SERCOM5, &config);

	spi_register_callback(&mSpi.instance, RJTUSBBridgeSPIM_callback, SPI_CALLBACK_BUFFER_TRANSCEIVED);

	spi_enable_callback(&mSpi.instance, SPI_CALLBACK_BUFFER_TRANSCEIVED);

	spi_enable(&mSpi.instance);

	*rsp_len = 0;

	mSpi.enabled = true;
	__mCurrentConfig = RJT_USB_CONFIG_SPI_MASTER;

	RJTLogger_print("CONFIG: SPIM");
	return RJT_USB_ERROR_NONE;
}


static void uninit_spi_master(void)
{
	ASSERT(true == mSpi.enabled);
	spi_reset(&mSpi.instance);

	RJTLogger_print("CONFIG: uninit spi master");

	// Call config gpio to reset all of the pinmux to gpio settings
	config_gpio();
	mSpi.enabled = false;
	__mCurrentConfig = RJT_USB_CONFIG_GPIO;
}


static void uninit_current_config(void)
{
	enum RJT_USB_CONFIG current_config = read_current_config();

	switch(current_config)
	{
		case RJT_USB_CONFIG_GPIO:
			RJTLogger_print("CONFIG: uninit gpio by calling config_gpio()");
			config_gpio();
			return;
		
		case RJT_USB_CONFIG_SPI_MASTER:
			RJTLogger_print("CONFIG: uninit spim");
			uninit_spi_master();
			return;

		default:
			ASSERT(false);
			break;
	}
}


void RJTUSBBridgeConfig_index2gpio(uint8_t index, bool * success, uint8_t * gpio)
{
	enum RJT_USB_CONFIG current_config = read_current_config();

	if(index < RJT_USB_BRIDGE_NUM_GPIOS) 
	{
		uint8_t gpio_num = config2gpio(current_config, index);

		if(0xff != gpio_num) 
		{
			*gpio = config2gpio(current_config, index);
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
	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t config;
	RJT_USB_BRIDGE_END_CMD


	uninit_current_config();

	RJTLogger_print("CONFIG: cmd.config = %d", cmd.config);

	switch(cmd.config)
	{
		case RJT_USB_CONFIG_GPIO:
			*rsp_len = 0;
			//RJTLogger_print("CONFIG: GPIO");
			return config_gpio();
		
		case RJT_USB_CONFIG_SPI_MASTER:
			//RJTLogger_print("CONFIG: SPI_MASTER");
			return config_spi_master(&cmd_data[sizeof(cmd)], cmd_len - sizeof(cmd), rsp_data, rsp_len);

		default:
			RJTLogger_print("CONFIG: Unknown cmd.config");
			return RJT_USB_ERROR_PARAMETER;
	}
}


void RJTUSBBridgeConfig_reset(void)
{
	uninit_current_config();

	config_gpio();
}


void RJTUSBBridgeConfig_init(void)
{
	config_gpio();
}
