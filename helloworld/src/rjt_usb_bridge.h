/*
 * rjt_usb_bridge.h
 *
 * Created: 11/14/2020 1:47:02 PM
 *  Author: robbytong
 */ 


#ifndef RJT_USB_BRIDGE_H_
#define RJT_USB_BRIDGE_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmsis_compiler.h>
#include <string.h>

#define RJT_USB_BRIDGE_NUM_GPIOS (8)

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


#define RJT_USB_CMD_DECL(_func_name_)		enum RJT_USB_ERROR _func_name_(const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)


RJT_USB_CMD_DECL(RJTUSBBridgeConfig_setConfig);

void RJTUSBBridgeConfig_index2gpio(uint8_t index, bool * success, uint8_t * gpio);

void RJTUSBBridge_configInit(void);


RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_configureIndex);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_pinSet);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_pinRead);



#define RJT_USB_BRIDGE_BEGIN_CMD					\
__PACKED_STRUCT Annonymous {

#define RJT_USB_BRIDGE_END_CMD						\
} cmd = {0};															\
if(cmd_len < sizeof(cmd)) {								\
	return RJT_USB_ERROR_MALFORMED_PACKET;	\
}																					\
memcpy(&cmd, cmd_data, sizeof(cmd));


#endif /* RJT_USB_BRIDGE_H_ */