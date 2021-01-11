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
		uint8_t bRequest,
		uint16_t wValue,
		uint8_t ** dst_buf,
		uint16_t * dst_buflen);


enum RJT_USB_ERROR {
	RJT_USB_ERROR_NONE             = 0x00,
	RJT_USB_ERROR_UNKNOWN_CMD      = 0x01,
	RJT_USB_ERROR_NO_MEMORY        = 0x02,
	RJT_USB_ERROR_MALFORMED_PACKET = 0x03,
	RJT_USB_ERROR_RESOURCE_BUSY    = 0x04,
	RJT_USB_ERROR_PARAMETER        = 0x05,
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
	USB_CMD_DFU_WRITE_BLOCK     = 0x0B,
	USB_CMD_DFU_READ_BLOCK      = 0x0C,
	USB_CMD_DFU_GET_STATUS      = 0x0D,
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
	CNTRL_REQ_RESET = 0x00
};


#endif /* RJT_USB_BRIDGE_H_ */