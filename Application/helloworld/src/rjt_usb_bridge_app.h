/*
 * rjt_usb_bridge_app.h
 *
 * Created: 11/14/2020 1:47:02 PM
 *  Author: robbytong
 */ 


#ifndef RJT_USB_BRIDGE_APP_H_
#define RJT_USB_BRIDGE_APP_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmsis_compiler.h>
#include <string.h>
#include <spi.h>

#include "rjt_usb_bridge.h"

#define RJT_USB_BRIDGE_NUM_GPIOS (16)


enum RJT_USB_INTERRUPT_BIT {
	RJT_USB_INTERRUPT_BIT_GPIO = 0x00,
	RJT_USB_INTERRUPT_BIT_SPI  = 0x01,
};

void RJTUSBBridge_setInterruptBit(enum RJT_USB_INTERRUPT_BIT bit, bool notify);

void RJTUSBBridge_clearInterruptBit(enum RJT_USB_INTERRUPT_BIT bit, bool notify);


#define RJT_USB_CMD_DECL(_func_name_)		enum RJT_USB_ERROR _func_name_(const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)


RJT_USB_CMD_DECL(RJTUSBBridgeConfig_setConfig);

struct spi_module * SKUSBBridgeConfig_getSpiModule(void);

struct i2c_master_module * SKUSBBridgeConfig_getI2CModule(void);

void RJTUSBBridgeConfig_gpio2index(uint8_t gpio, bool * success, uint8_t * index);

void RJTUSBBridgeConfig_index2gpio(uint8_t index, bool * success, uint8_t * gpio);

void RJTUSBBridgeConfig_index2extint(uint8_t index, bool * success, uint8_t * extint);

void RJTUSBBridgeConfig_reset(void);

void RJTUSBBridgeConfig_init(void);


RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_enablePinInterrupt);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_disablePinInterrupt);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_configureIndex);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_pinSet);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_pinRead);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_getInterruptStatus);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_clearInterruptStatus);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_parallelWrite);

RJT_USB_CMD_DECL(RJTUSBBridgeDFU_reset);

RJT_USB_CMD_DECL(RJTUSBBridgeGPIO_setLed);



void RJTUSBBridgeGPIO_init(void);


RJT_USB_CMD_DECL(RJTUSBBridgeSPIM_transferData);

void RJTUSBBridgeSPIM_callback(struct spi_module * const module);


RJT_USB_CMD_DECL(SKUSBBridgeI2CM_transaction);


#define RJT_USB_BRIDGE_BEGIN_CMD	\
__PACKED_STRUCT Annonymous {

#define RJT_USB_BRIDGE_END_CMD							\
} cmd = {0};											\
if(cmd_len < sizeof(cmd)) {								\
	RJTLogger_print("not enough data for packet");		\
	*rsp_len = 0;										\
	return RJT_USB_ERROR_MALFORMED_PACKET;				\
}														\
memcpy(&cmd, cmd_data, sizeof(cmd));


#endif /* RJT_USB_BRIDGE_H_ */