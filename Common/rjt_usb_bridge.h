/*
 * rjt_usb_bridge.h
 *
 * Created: 1/10/2021 5:32:04 PM
 *  Author: robbytong
 */ 


#ifndef RJT_USB_BRIDGE_H_
#define RJT_USB_BRIDGE_H_


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <cmsis_compiler.h>

void RJTUSBBridge_init(void);

void RJTUSBBridge_processCmd(const uint8_t * cmd_data, size_t cmd_len,
		bool * send_cached_rsp, uint8_t * rsp_data, size_t * rsp_len);


void RJTUSBBridge_rspSent(void);

bool RJTUSBBridge_processControlRequestWrite(
		uint8_t bmRequest,
		uint8_t bRequest,
		uint16_t wValue,
		uint8_t ** dst_buf,
		uint16_t * dst_buflen);

bool RJTUSBBridge_processControlRequestRead(
		uint8_t bmRequest,
		uint8_t bRequest,
		uint16_t wValue,
		uint8_t ** dst_buf,
		uint16_t * dst_buflen);


enum RJT_USB_BRIDGE_MODE {
	RJT_USB_BRIDGE_MODE_APP = 0x00,
	RJT_USB_BRIDGE_MODE_DFU = 0x01,
	RJT_USB_BRIDGE_MODE_MAX,
};


enum RJT_USB_ERROR {
	RJT_USB_ERROR_NONE             = 0x00,
	RJT_USB_ERROR_UNKNOWN_CMD      = 0x01,
	RJT_USB_ERROR_NO_MEMORY        = 0x02,
	RJT_USB_ERROR_MALFORMED_PACKET = 0x03,
	RJT_USB_ERROR_RESOURCE_BUSY    = 0x04,
	RJT_USB_ERROR_PARAMETER        = 0x05,
	RJT_USB_ERROR_STATE            = 0x06,
	RJT_USB_ERROR_OPERATION_FAILED = 0x07,
};


enum RJT_USB_CONFIG {
	RJT_USB_CONFIG_GPIO       = 0x00,
	RJT_USB_CONFIG_I2C_MASTER = 0x01,
	RJT_USB_CONFIG_I2C_SLAVE  = 0x02,
	RJT_USB_CONFIG_SPI_MASTER = 0x03,
	RJT_USB_CONFIG_SPI_SLAVE  = 0x04,
	RJT_USB_CONFIG_UART       = 0x05,
	RJT_USB_CONFIG_MAX,
};


void RJTUSBBridge_setInterruptStatus(uint32_t interrupt_status);



enum USBCmd
{
	USB_CMD_ECHO           = 0x01,
	USB_CMD_CFG            = 0x02,
	USB_CMD_GPIO_CFG       = 0x03,
	USB_CMD_GPIO_PIN_SET   = 0x04,
	USB_CMD_GPIO_PIN_READ  = 0x05,
	USB_CMD_GPIO_GET_INTERRUPT_STATUS    = 0x06,
	USB_CMD_GPIO_CLEAR_INTERRUPT_STATUS  = 0x07,
	USB_CMD_GPIO_ENABLE_PIN_INTERRUPT    = 0x08,
	USB_CMD_GPIO_PARALLEL_WRITE = 0x09,
	USB_CMD_SPIM_TRANSFER_DATA  = 0x0A,
	
	USB_CMD_DFU_START           = 0x0B,
	/**
		Initiates a DFU update. This command erases the 
		application flash memory area. It also resets the 
		flash counter to the application flash base.

		Parameters:
		uint32_t image_size:	the expected image size
		uint32_t expected_crc: the expected crc

		- RJT_USB_ERROR_MALFORMED_PACKET if not enough data
		- RJT_USB_ERROR_NO_MEMORY if given image size is greater than nvm space
		- RJT_USB_ERROR_NONE success
	 */

	USB_CMD_DFU_WRITE_DATA     = 0x0C,
	/**
		Writes the data provided to flash. Internally, data
		is buffered, and once it reaches the size of a page,
		(64 bytes), it is written to flash. The flash counter
		is also incremented automatically.

		Parameters:
		uint8_t[] firmware data (entire packet)

		- RJT_USB_ERROR_STATE if USB_CMD_DFU_START has not been sent
		- RJT_USB_ERROR_NO_MEMORY if write has exceeded expected image size
		- RJT_USB_ERROR_NONE success
	 */
	
	USB_CMD_DFU_READ_DATA      = 0x0D,
	/**
		Reads data from the flash. After reading, the read
		flash counter is incremented automatically.
	 */

	USB_CMD_DFU_RESET_READ_PTR	= 0x0E,
	/**
		Resets the read pointer to the beginning of the 
		application base pointer.
	 */

	USB_CMD_DFU_DONE_WRITING = 0x0F,
	/**
		Finalizes the DFU update. Any remaining data stored
		in the cache is written to NVM.
	 */

	USB_CMD_DFU_RESET = 0x10,
	/**
	  Initiates a soft reset to run the newly programmed application.

		Parameters:
		uint8_t fw_mode: 
			value RJT_USB_BRIDGE_MODE_DFU forces the system to restart into bootloader mode.
			value RJT_USB_BRIDGE_MODE_APP puts the system into normal mode. The bootloader
				will run the application if there is one in memory. Otherwise, it will stay in
				bootloader mode.

		- RJT_USB_ERROR_PARAMETER if an invalid mode was given
	 */
};


__PACKED_STRUCT USBHeader
{
	uint8_t seq_no:1;
	uint8_t cmd:7;
	uint8_t error;
	uint8_t data[];
};

typedef struct USBHeader USBHeader;



enum CNTRL_REQ {
	CNTRL_REQ_RESET = 0x00,
	CNTRL_REQ_MODE  = 0x01,
};


#endif /* RJT_USB_BRIDGE_H_ */