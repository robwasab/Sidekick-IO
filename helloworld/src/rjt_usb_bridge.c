/*
 * rjt_usb_bridge.c
 *
 * Created: 11/14/2020 1:46:33 PM
 *  Author: robbytong
 */ 

#include "rjt_usb_bridge.h"
#include "rjt_logger.h"
#include "utils.h"

#include <stdbool.h>
#include <string.h>
#include <asf.h>
#include <samd21.h>

static bool mSeqNo = false;


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
	USB_CMD_SPIM_TRANSFER_DATA = 0x0A,
};


__PACKED_STRUCT USBHeader
{
	uint8_t seq_no:1;
	uint8_t cmd:7;
	uint8_t error;
	uint8_t data[];
};

typedef struct USBHeader USBHeader;

static uint32_t interrupt_status = 0;


void RJTUSBBridge_setInterruptBit(enum RJT_USB_INTERRUPT_BIT bit, bool notify)
{
	system_interrupt_enter_critical_section();
	interrupt_status |= (1 << bit);
	system_interrupt_leave_critical_section();

	if(notify) {
		RJTUSBBridge_setInterruptStatus(interrupt_status);
	}
};


void RJTUSBBridge_clearInterruptBit(enum RJT_USB_INTERRUPT_BIT bit, bool notify)
{
	system_interrupt_enter_critical_section();
	interrupt_status &= ~(1 << bit);
	system_interrupt_leave_critical_section();
	
	if(notify) {
		RJTUSBBridge_setInterruptStatus(interrupt_status);
	}
};


#if 0
static bool in_array(uint8_t value, const uint8_t * arr, size_t len)
{	
	size_t k;

	for(k = 0; k < len; k++)
	{
		if(value == arr[k]) {
			break;
		}
	}
	
	if(len == k) {
		return false;
	}
	return true;
}
#endif


static enum RJT_USB_ERROR process_cmd_echo(const uint8_t * cmd_data, size_t cmd_len, 
		uint8_t * rsp_data, size_t * rsp_len)
{
	ASSERT(cmd_len <= *rsp_len);

	//RJTLogger_print("cmd echo");

	memcpy(rsp_data, cmd_data, cmd_len);

	*rsp_len = cmd_len;

	return RJT_USB_ERROR_NONE;
};


void RJTUSBBridge_processCmd(const uint8_t * cmd_data, size_t cmd_len, 
		bool * send_cached_rsp, uint8_t * rsp_data, size_t * rsp_len)
{
	if(cmd_len < 1) {
		*send_cached_rsp = true;
		return;
	}

	enum RJT_USB_ERROR ret_code;

	USBHeader * cmd_header = (USBHeader *) cmd_data;
	USBHeader * rsp_header = (USBHeader *) rsp_data;

	
	#if 1
	// check the sequence number
	if(mSeqNo == cmd_header->seq_no) {
		// This is old data
		RJTLogger_print("old sequence number");
		*send_cached_rsp = true;
		return;
	}
	#endif

	// the command is good!
	*send_cached_rsp = false;

	// update sequence number
	mSeqNo = cmd_header->seq_no;

	// Reserve the first 2 bytes of the response for adding in
	// the response header and an error code
	ASSERT(*rsp_len >= sizeof(USBHeader));
	*rsp_len -= sizeof(USBHeader);

	#define CASE2FUNC(cmd_id, cmd_func)																				\
		case cmd_id:																														\
			ret_code = cmd_func(cmd_header->data, cmd_len - sizeof(USBHeader),		\
					rsp_header->data, rsp_len);																				\
			break										

	//RJTLogger_print("USB Bridge: Processing %x", cmd_header->cmd);

	switch(cmd_header->cmd)
	{
		CASE2FUNC(USB_CMD_ECHO, process_cmd_echo);

		CASE2FUNC(USB_CMD_CFG, RJTUSBBridgeConfig_setConfig);

		CASE2FUNC(USB_CMD_GPIO_CFG, RJTUSBBridgeGPIO_configureIndex);

		CASE2FUNC(USB_CMD_GPIO_PIN_SET, RJTUSBBridgeGPIO_pinSet);

		CASE2FUNC(USB_CMD_GPIO_PIN_READ, RJTUSBBridgeGPIO_pinRead);

		CASE2FUNC(USB_CMD_GPIO_GET_INTERRUPT_STATUS, RJTUSBBridgeGPIO_getInterruptStatus);

		CASE2FUNC(USB_CMD_GPIO_CLEAR_INTERRUPT_STATUS, RJTUSBBridgeGPIO_clearInterruptStatus);

		CASE2FUNC(USB_CMD_GPIO_ENABLE_PIN_INTERRUPT, RJTUSBBridgeGPIO_enablePinInterrupt);

		CASE2FUNC(USB_CMD_SPIM_TRANSFER_DATA, RJTUSBBridgeSPIM_transferData);

		CASE2FUNC(USB_CMD_GPIO_PARALLEL_WRITE, RJTUSBBridgeGPIO_parallelWrite);

		default:
			ret_code = RJT_USB_ERROR_UNKNOWN_CMD;
			break;
	}

	// Response format is always:
	// [0] replica of the command header that generated this response
	// [1] error code
	// ... rest of command data
	*rsp_header = *cmd_header;
	rsp_header->error = ret_code;
	*rsp_len += sizeof(USBHeader);
};


void RJTUSBBridge_rspSent(void)
{
	//RJTLogger_print("rsp sent");
}


enum CNTRL_REQ {
	CNTRL_REQ_RESET = 0x00
};


bool RJTUSBBridge_processControlRequestWrite(
		uint8_t bRequest, 
		uint16_t wValue, 
		uint8_t ** dst_buf, 
		uint16_t * dst_buflen)
{
	switch(bRequest)
	{
		case CNTRL_REQ_RESET: {
			mSeqNo = false;

			// This command expects no additional write data...
			*dst_buf = NULL;
			*dst_buflen = 0;

			RJTUSBBridgeConfig_reset();
			
			RJTLogger_print("Resetting usb bridge...");
		} break;

		default:
			RJTLogger_print("Unknown cntrl req type: %x", bRequest);
			return false;
	}

	return true;
}


void RJTUSBBridge_init(void)
{
	RJTUSBBridgeConfig_init();
	RJTUSBBridgeGPIO_init();
}
