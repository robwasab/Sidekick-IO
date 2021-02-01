/*
 * rjt_usb_bridge_boot.h
 *
 * Created: 1/24/2021 5:10:41 PM
 *  Author: robbytong
 */ 
#ifndef __RJT_USB_BRIDGE_BOOT_H__
#define __RJT_USB_BRIDGE_BOOT_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmsis_compiler.h>
#include <string.h>

#include "rjt_usb_bridge.h"

#define RJT_USB_CMD_DECL(_func_name_)		enum RJT_USB_ERROR _func_name_(const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)

RJT_USB_CMD_DECL(RJTUSBBridgeDFU_start);

RJT_USB_CMD_DECL(RJTUSBBridgeDFU_writeData);

RJT_USB_CMD_DECL(RJTUSBBridgeDFU_readData);

RJT_USB_CMD_DECL(RJTUSBBridgeDFU_resetReadPtr);

RJT_USB_CMD_DECL(RJTUSBBridgeDFU_doneWriting);

RJT_USB_CMD_DECL(RJTUSBBridgeDFU_reset);

void RJTUSBBridgeDFU_init(void);


#define RJT_USB_BRIDGE_BEGIN_CMD					\
__PACKED_STRUCT Annonymous {

#define RJT_USB_BRIDGE_END_CMD						\
} cmd = {0};															\
if(cmd_len < sizeof(cmd)) {								\
	RJTLogger_print("not enough data for packet");\
	*rsp_len = 0;														\
	return RJT_USB_ERROR_MALFORMED_PACKET;	\
}																					\
memcpy(&cmd, cmd_data, sizeof(cmd));


#endif
