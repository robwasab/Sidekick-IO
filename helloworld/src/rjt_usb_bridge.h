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

void RJTUSBBridge_init(void);

void RJTUSBBridge_processCmd(const uint8_t * cmd_data, size_t cmd_len, 
		bool * send_cached_rsp, uint8_t * rsp_data, size_t * rsp_len);


void RJTUSBBridge_rspSent(void);

bool RJTUSBBridge_processControlRequestWrite(
		uint8_t bRequest, 
		uint16_t wValue, 
		uint8_t ** dst_buf, 
		uint16_t * dst_buflen);


#endif /* RJT_USB_BRIDGE_H_ */