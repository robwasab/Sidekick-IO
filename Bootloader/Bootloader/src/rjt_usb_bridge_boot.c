/*
 * rjt_usb_bridge_boot.c
 *
 * Created: 1/10/2021 5:48:10 PM
 *  Author: robbytong
 */ 

#include "rjt_usb_bridge.h"
#include "rjt_usb_bridge_boot.h"
#include "rjt_logger.h"
#include "utils.h"

#include <stdbool.h>
#include <string.h>
#include <asf.h>
#include <samd21.h>

static bool mSeqNo = false;


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
	case cmd_id:																															\
		ret_code = cmd_func(cmd_header->data, cmd_len - sizeof(USBHeader),			\
		rsp_header->data, rsp_len);																							\
		break

	
	switch(cmd_header->cmd)
	{
		CASE2FUNC(USB_CMD_DFU_START,          RJTUSBBridgeDFU_start);
		CASE2FUNC(USB_CMD_DFU_WRITE_DATA,     RJTUSBBridgeDFU_writeData);
		CASE2FUNC(USB_CMD_DFU_READ_DATA,      RJTUSBBridgeDFU_readData);
		CASE2FUNC(USB_CMD_DFU_RESET_READ_PTR, RJTUSBBridgeDFU_resetReadPtr);
		CASE2FUNC(USB_CMD_DFU_DONE_WRITING,   RJTUSBBridgeDFU_doneWriting);

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
			mSeqNo = false;

			// This command expects no additional write data...
			*dst_buf = NULL;
			*dst_buflen = 0;

			
			RJTLogger_print("Resetting usb bridge...");
		} break;

		case CNTRL_REQ_MODE: {
			
			if(bmRequest & USB_REQ_DIR_IN) {
				// host is reading

				if(bmRequest & USB_REQ_TYPE_VENDOR) {
					// vendor request

					if(bmRequest & USB_REQ_RECIP_INTERFACE) {
						// request to the interface
						static uint8_t mode = RJT_USB_BRIDGE_MODE_DFU;
						*dst_buf = &mode;
						*dst_buflen = sizeof(mode);

						RJTLogger_print("Mode: DFU");
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
						static uint8_t mode = RJT_USB_BRIDGE_MODE_DFU;
						*dst_buf = &mode;
						*dst_buflen = sizeof(mode);

						RJTLogger_print("Mode: DFU");
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
	RJTUSBBridgeDFU_init();
}
