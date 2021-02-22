/*
 * sk_usb_bridge_i2c_master.c
 *
 * Created: 2/21/2021 1:21:58 PM
 *  Author: robbytong
 */ 

#include "rjt_usb_bridge_app.h"
#include "rjt_logger.h"
#include "utils.h"

#include <port.h>
#include <stdbool.h>
#include <asf.h>
#include <i2c_master.h>

#define MAX_ATTEMPTS	1000



enum I2CM_CMD
{
	I2CM_CMD_WRITE_DATA = 'w',
	I2CM_CMD_WRITE_BYTE = 'W',
	I2CM_CMD_READ_DATA = 'r',
	I2CM_CMD_STOP = '.',
};


/**
 * Write data over i2c without sending a stop
 * If an error occurs, this function returns RJT_USB_ERROR_OPERATION_FAILED and
 * the rsp_data is populated with the expectation that the current usb processing
 * function returns immediately with the rsp_data.
 */
static enum RJT_USB_ERROR write_i2c_data(
	struct i2c_master_module * i2c_handle,
	uint8_t slave_addr,
	const uint8_t * data, 
	uint8_t len, 
	uint8_t * rsp_data, 
	size_t * rsp_len)
{
	__PACKED_STRUCT annonymous {
		uint8_t len;
		uint8_t type;
		uint8_t sk_error;
		uint8_t asf_error;
	} rsp = {0};
	
	if(*rsp_len < sizeof(rsp)) {
		RJTLogger_print("I2CM: write_i2c_data not enough data for rsp. rsp_len: %d", *rsp_len);
		*rsp_len = 0;
		return RJT_USB_ERROR_NO_MEMORY;
	}
	
	struct i2c_master_packet packet = {
		.address     = slave_addr,
		.data_length = len,
		.data        = data,
		.ten_bit_address = false,
		.high_speed      = false,
		.hs_master_code  = 0x00,
	};
	
	enum status_code res = 
		i2c_master_write_packet_wait_no_stop(i2c_handle, &packet);

	rsp.len = sizeof(rsp) - 1;
	rsp.type = I2CM_CMD_WRITE_DATA;
	rsp.sk_error = (res == STATUS_OK) ? RJT_USB_ERROR_NONE : RJT_USB_ERROR_OPERATION_FAILED;
	rsp.asf_error = res;

	*rsp_len = sizeof(rsp);
	
	memcpy(rsp_data, &rsp, sizeof(rsp));
	
	//if(STATUS_OK != res) {
	//	RJTLogger_print("I2CM: ASF write error: %xh", res);
	//}
	
	return rsp.sk_error;
}


static enum RJT_USB_ERROR read_i2c_data(
	struct i2c_master_module * i2c_handle,
	uint8_t slave_addr,
	uint8_t readlen,
	uint8_t * rsp_data,
	size_t * rsp_len)
{
	__PACKED_STRUCT annonymous {
		uint8_t len;
		uint8_t type;
		uint8_t sk_error;
		uint8_t asf_error;
	} rsp_header = {0};
	
	if(*rsp_len < readlen + sizeof(rsp_header)) {
		RJTLogger_print("I2CM: read_i2c_data not enough data for rsp: readlen = %d, rsp_len = %d", readlen, *rsp_len);
		*rsp_len = 0;
		return RJT_USB_ERROR_NO_MEMORY;
	}
	
	uint8_t * read_dst = &rsp_data[sizeof(rsp_header)];
	
	struct i2c_master_packet packet = {
		.address     = slave_addr,
		.data_length = readlen,
		.data        = read_dst,
		.ten_bit_address = false,
		.high_speed      = false,
		.hs_master_code  = 0x00,
	};
	
	enum status_code res =
		i2c_master_read_packet_wait(i2c_handle, &packet);
	
	rsp_header.type = I2CM_CMD_READ_DATA;
	rsp_header.sk_error = (res == STATUS_OK) ? RJT_USB_ERROR_NONE : RJT_USB_ERROR_OPERATION_FAILED;
	rsp_header.asf_error = res;

	rsp_header.len = readlen + sizeof(rsp_header) - 1;
	*rsp_len = sizeof(rsp_header) + readlen;
	
	//RJTLogger_print("read rsp.len: %d", rsp_header.len);
	
	memcpy(rsp_data, &rsp_header, sizeof(rsp_header));
	
	return rsp_header.sk_error;
}


enum RJT_USB_ERROR SKUSBBridgeI2CM_transaction(const uint8_t * cmd_data, size_t cmd_len,
		uint8_t * const rsp_data, size_t * const rsp_len)
{
	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t slave_addr;
		uint8_t fmt_str_len;
	RJT_USB_BRIDGE_END_CMD

	size_t max_rsp_len = *rsp_len;
	
	// commands will increment the rsp_len variable as they add data to the response
	*rsp_len = 0;
	
	//RJTLogger_print("max_rsp_len: %d", max_rsp_len);

	// First, get the i2c master object
	struct i2c_master_module * i2c_handle =
		SKUSBBridgeConfig_getI2CModule();
	
	if(NULL == i2c_handle) {
		RJTLogger_print("I2CM: i2c not configured");
		return RJT_USB_ERROR_STATE;
	}


	// Offset by the amount of data already in the command struct
	cmd_len -= sizeof(cmd);
	cmd_data += sizeof(cmd);
	
	
	// Define 2 macros that help us:
	// - consume an arrays worth of data from the command data
	// - consume a byte from the command data
	#define CONSUME_ARRAY(arr_ptr, arr_len) \
	do { \
		if(cmd_len < arr_len) { \
			RJTLogger_print("I2CM: not enough data to read array. cmd_len %d, arr_len %d", cmd_len, arr_len); \
			return RJT_USB_ERROR_MALFORMED_PACKET; \
		} \
		arr_ptr = cmd_data; \
		cmd_data += arr_len; \
		cmd_len  -= arr_len; \
	} while(0)
	
	
	#define CONSUME_BYTE(dst) \
	do { \
		if(cmd_len == 0) { \
			RJTLogger_print("I2CM: not enough data to read byte"); \
		} \
		dst = *cmd_data++; \
		cmd_len -= 1; \
	} while(0)
	
	
	#define GET_RESPONSE_ARRAY(arr_ptr, arr_len) \
	do { \
		ASSERT(*rsp_len <= max_rsp_len); \
		arr_ptr = &rsp_data[*rsp_len]; \
		arr_len = max_rsp_len - *rsp_len; \
	} while(0)
	
	
	
	// Read the format string
	const uint8_t * fmt_str;
	CONSUME_ARRAY(fmt_str, cmd.fmt_str_len);
	
	
	// parse the format string
	for(size_t k = 0; k < cmd.fmt_str_len; k++)
	{
		switch(fmt_str[k])
		{
			case I2CM_CMD_WRITE_BYTE: {
				//RJTLogger_print("I2CM: writing byte");
				
				uint8_t writebyte;
				CONSUME_BYTE(writebyte);
				
				uint8_t * rsp_arr;
				size_t rsp_arrlen;
				GET_RESPONSE_ARRAY(rsp_arr, rsp_arrlen);
				
				enum RJT_USB_ERROR error =
					write_i2c_data(i2c_handle, cmd.slave_addr, 
						&writebyte, 1, 
						rsp_arr, &rsp_arrlen);
				
				*rsp_len += rsp_arrlen;
				
				if(RJT_USB_ERROR_NONE != error) {
					RJTLogger_print("I2CM: error while writing a byte");
					i2c_master_send_stop(i2c_handle);
					return error;
				}
				// else, continue processing the format string...
			} break;
			
			case I2CM_CMD_WRITE_DATA: {
				uint8_t writelen;
				CONSUME_BYTE(writelen);
				
				//RJTLogger_print("I2CM: writing data len %d", writelen);
				
				
				const uint8_t * writedata;
				CONSUME_ARRAY(writedata, writelen);
				
				uint8_t * rsp_arr;
				size_t  rsp_arrlen;
				GET_RESPONSE_ARRAY(rsp_arr, rsp_arrlen);
				
				enum RJT_USB_ERROR error =
					write_i2c_data(i2c_handle, cmd.slave_addr, 
						writedata, writelen, 
						rsp_arr, &rsp_arrlen);
				
				*rsp_len += rsp_arrlen;
				
				if(RJT_USB_ERROR_NONE != error) {
					RJTLogger_print("I2CM: error while writing array");
					i2c_master_send_stop(i2c_handle);
					return error;
				}
				// else, continue processing the format string...
			} break;
			
			
			case I2CM_CMD_READ_DATA: {
				uint8_t readlen;
				CONSUME_BYTE(readlen);
				
				//RJTLogger_print("I2CM: readlen: %d", readlen);
				
				uint8_t * rsp_arr;
				size_t  rsp_arrlen;
				GET_RESPONSE_ARRAY(rsp_arr, rsp_arrlen);
			
				enum RJT_USB_ERROR error =
					read_i2c_data(i2c_handle, cmd.slave_addr,
					readlen,
					rsp_arr, &rsp_arrlen);
			
				*rsp_len += rsp_arrlen;
				
				
				if(RJT_USB_ERROR_NONE != error) {
					RJTLogger_print("I2CM: error while reading array");
					return error;
				}
				// else, continue processing the format string...
			} break;
			
			
			case I2CM_CMD_STOP: {
				i2c_master_send_stop(i2c_handle);
			} break;
			
			
			default:
				return RJT_USB_ERROR_PARAMETER;
		}
	}

	//RJTLogger_print("rsp_len: %d", *rsp_len);
	return RJT_USB_ERROR_NONE;
}