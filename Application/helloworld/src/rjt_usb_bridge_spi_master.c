/*
 * rjt_usb_bridge_spi_master.c
 *
 * Created: 12/13/2020 2:04:10 AM
 *  Author: robbytong
 */ 

#include "rjt_usb_bridge_app.h"
#include "rjt_logger.h"
#include "utils.h"

#include <port.h>
#include <stdbool.h>
#include <asf.h>
#include <spi.h>

void RJTUSBBridgeSPIM_callback(struct spi_module * const module)
{
	RJTLogger_print("spi master callback!");

	enum status_code status = spi_get_job_status(module);

	switch(status) {
		case STATUS_OK:
			RJTLogger_print("STATUS_OK");
			break;
		case STATUS_BUSY:
			RJTLogger_print("ERR_BUSY");
			break;
		case STATUS_ERR_DENIED:
			RJTLogger_print("ERR_DENIED");
			break;
		case STATUS_ERR_INVALID_ARG:
			RJTLogger_print("ERR_INVALID_ARG");
			break;
		default:
			RJTLogger_print("Unknown SPI Error");
			break;
	}
}


enum RJT_USB_ERROR RJTUSBBridgeSPIM_transferData(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * rsp_data, size_t * rsp_len)
{
	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t ss_index;
	RJT_USB_BRIDGE_END_CMD

	//RJTLogger_print("SPIM: cmd_len = %d", cmd_len);

	// Check if we have enough response buffer space 
	size_t datalen = cmd_len - sizeof(cmd);

	if(*rsp_len < datalen) {
		RJTLogger_print("SPIM: cmd.datalen too big. Bigger than response buffer");
		*rsp_len = 0;
		return RJT_USB_ERROR_PARAMETER;
	}

	// Get the spi handle from the configuration module
	struct spi_module * spi_handle = 
		RJTUSBBridgeConfig_getSpiModule();

	if(NULL == spi_handle) {
		RJTLogger_print("SPIM: spi not configured");
		*rsp_len = 0;
		return RJT_USB_ERROR_RESOURCE_BUSY;
	}

	// Get the pointer to the tx data
	// offset by cmd header...
	const uint8_t * tx_data = &cmd_data[sizeof(cmd)];

	// Set the chip select line
	bool success = false;
	uint8_t gpio = 0xff;

	if(0xff != cmd.ss_index) {
		RJTUSBBridgeConfig_index2gpio(cmd.ss_index, &success, &gpio);
	}
	else {
		// 0xff means no chip select was provided, and use the default one
		success = true;
		gpio = 0xff;
	}

	if(true == success)
	{
		// drive low
		if(0xff != gpio) {
			//RJTLogger_print("SPIM: asserting index %d, pin %d", cmd.ss_index, gpio);
			port_pin_set_output_level(gpio, false);
		}


		// transfer data
		enum status_code status =
		spi_transceive_buffer_wait(spi_handle, (uint8_t *)tx_data, rsp_data, datalen);
	

		if(0xff != gpio) {
			// drive high
			port_pin_set_output_level(gpio, true);
		}

		// handle error code
		if(STATUS_OK != status) {
			RJTLogger_print("SPIM: error code: %d", status);
			*rsp_data = status;
			*rsp_len = sizeof(status);
			return RJT_USB_ERROR_RESOURCE_BUSY;
		}
		else {
			//RJTLogger_print("SPIM: success!");
			*rsp_len = datalen;
			return RJT_USB_ERROR_NONE;
		}
	}
	else {
		RJTLogger_print("SPIM: error invalid gpio index");
		*rsp_len = 0;
		return RJT_USB_ERROR_PARAMETER;
	}
}
