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

static bool mSeqNo = false;


enum USBCmd
{
	USB_CMD_ECHO = 0x01,
};


struct USBHeader
{
	uint8_t seq_no:1;
	uint8_t cmd:7;
	uint8_t error;
	uint8_t data[];
};

typedef struct USBHeader USBHeader;

enum USB_ERROR{
	USB_ERROR_NONE             = 0x00,
	USB_ERROR_UNKNOWN_CMD      = 0x01,
	USB_ERROR_NO_MEMORY        = 0x02,
	USB_ERROR_MALFORMED_PACKET = 0x03,
	USB_ERROR_RESOURCE_BUSY    = 0x04,
};


static enum USB_ERROR process_cmd_echo(const uint8_t * cmd_data, size_t cmd_len, 
		uint8_t * rsp_data, size_t * rsp_len)
{
	ASSERT(cmd_len <= *rsp_len);

	//RJTLogger_print("cmd echo");

	memcpy(rsp_data, cmd_data, cmd_len);

	*rsp_len = cmd_len;

	return USB_ERROR_NONE;
};


void RJTUSBBridge_processCmd(const uint8_t * cmd_data, size_t cmd_len, 
		bool * send_cached_rsp, uint8_t * rsp_data, size_t * rsp_len)
{
	if(cmd_len < 1) {
		*send_cached_rsp = true;
		return;
	}

	enum USB_ERROR ret_code;

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

	switch(cmd_header->cmd)
	{
		case USB_CMD_ECHO:
			ret_code = process_cmd_echo(cmd_header->data, cmd_len - sizeof(USBHeader), 
					rsp_header->data, rsp_len);
			break;

		default:
			ret_code = USB_ERROR_UNKNOWN_CMD;
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
	
}
