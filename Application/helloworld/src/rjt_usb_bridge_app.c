/*
 * rjt_usb_bridge.c
 *
 * Created: 11/14/2020 1:46:33 PM
 *  Author: robbytong
 */ 

#include "rjt_usb_bridge_app.h"
#include "rjt_logger.h"
#include "utils.h"

#include <stdbool.h>
#include <string.h>
#include <asf.h>
#include <samd21.h>

static bool mSeqNo = false;

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

	//RJTLogger_print("cmd echo: %d", cmd_len);

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

		CASE2FUNC(USB_CMD_DFU_RESET, RJTUSBBridgeDFU_reset);

		CASE2FUNC(USB_CMD_I2CM_TRANSACTION, SKUSBBridgeI2CM_transaction);
		
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


bool RJTUSBBridge_processControlRequestWrite(
		uint8_t bmRequest,
		uint8_t bRequest,
		uint16_t wValue,
		uint8_t ** dst_buf,
		uint16_t * dst_buflen)
{
	switch(bRequest)
	{
		case CNTRL_REQ_RESET: {
			RJTLogger_print("Resetting usb bridge...");

			mSeqNo = false;

			// This command expects no additional write data...
			*dst_buf = NULL;
			*dst_buflen = 0;

			RJTUSBBridgeConfig_reset();			
		} break;

		default:
			RJTLogger_print("Unknown cntrl req type: %x", bRequest);
			return false;
	}

	return true;
}


bool RJTUSBBridge_processControlRequestRead(
		uint8_t bmRequest,
		uint8_t bRequest,
		uint16_t wValue,
		uint8_t ** dst_buf,
		uint16_t * dst_buflen)
{
	switch(bRequest)
	{
		case CNTRL_REQ_MODE: {
			
			if(bmRequest & USB_REQ_DIR_IN) {
				// host is reading

				if(bmRequest & USB_REQ_TYPE_VENDOR) {
					// vendor request

					if(bmRequest & USB_REQ_RECIP_INTERFACE) {
						// request to the interface
						static uint8_t mode = RJT_USB_BRIDGE_MODE_APP;
						*dst_buf = &mode;
						*dst_buflen = sizeof(mode);

						RJTLogger_print("Mode: APP");
						return true;
					}
				}
			}
			RJTLogger_print("Unknown ctrl request (mode)...");
			return false;
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
