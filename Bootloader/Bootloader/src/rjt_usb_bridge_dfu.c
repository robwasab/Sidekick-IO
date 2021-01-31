/*
 * rjt_usb_bridge_dfu.c
 *
 * Created: 1/24/2021 5:05:26 PM
 *  Author: robbytong
 */

#include "rjt_usb_bridge_boot.h"
#include "rjt_logger.h"
#include "bootloader.h"
#include "utils.h"

#include <asf.h>
#include <nvm.h>

#define SK_NVM_SIZE_BYTES				(0x00040000)	// 256kB
#define SK_NVM_SIZE_PAGES				(SK_NVM_SIZE_BYTES / NVMCTRL_PAGE_SIZE)
#define SK_NVM_SIZE_ROWS				(SK_NVM_SIZE_PAGES / NVMCTRL_ROW_PAGES)

#pragma GCC push_options
#pragma GCC optimize("O0")


static void erase_application(void)
{
	uint32_t start_addr = SK_APP_ADDR;

	enum status_code status;

	for(uint32_t erase_addr = start_addr; erase_addr < SK_NVM_SIZE_BYTES; )
	{
		do {
			status = nvm_erase_row(erase_addr);
		} while(status == STATUS_BUSY);

		erase_addr += NVMCTRL_PAGE_SIZE * NVMCTRL_ROW_PAGES;
	}
}


struct DFUContext
{
	size_t current_write_addr;
	uint8_t buffer[NVMCTRL_PAGE_SIZE];
	size_t  bufcnt;

	size_t current_read_addr;
};


static struct DFUContext mDFU;


static void reset_dfu_context(struct DFUContext * self)
{
	ASSERT(NULL != self);

	memset(self, 0, sizeof(struct DFUContext));

	self->current_write_addr = SK_APP_ADDR;
	self->current_read_addr = SK_APP_ADDR;
}


static bool enqueue_dfu_data(
		struct DFUContext * self, const uint8_t * data, size_t len)
{
	ASSERT(NULL != self);

	if(SK_NVM_SIZE_BYTES <= self->current_write_addr + len) {
		return false;
	}

	while(0 < len) {
		size_t space_avail = NVMCTRL_PAGE_SIZE - self->bufcnt;

		size_t num2cpy = MIN(space_avail, len);

		memcpy(&self->buffer[self->bufcnt], data, num2cpy);
		self->bufcnt += num2cpy;
		len -= num2cpy;
		data += num2cpy;

		if(0 == space_avail) {
			enum status_code status;
			do {
				//RJTLogger_print("Writting buffer...");
				//RJTLogger_process();

				status = nvm_write_buffer(
						self->current_write_addr, self->buffer, NVMCTRL_PAGE_SIZE);
			} while(status == STATUS_BUSY);

			self->current_write_addr += NVMCTRL_PAGE_SIZE;
			self->bufcnt = 0;
		}
	}
	return true;
}


static void finalize_dfu(struct DFUContext * self)
{
	ASSERT(NULL != self);
	enum status_code status;

	do {
		status = nvm_write_buffer(
				self->current_write_addr, self->buffer, self->bufcnt);
	} while(status == STATUS_BUSY);

	reset_dfu_context(self);
}


enum RJT_USB_ERROR RJTUSBBridgeDFU_start(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	RJT_USB_BRIDGE_BEGIN_CMD
		uint32_t image_size;
		uint32_t crc;
	RJT_USB_BRIDGE_END_CMD

	RJTLogger_print("DFU: start");
	
	if(cmd.image_size > (SK_NVM_SIZE_BYTES - SK_APP_ADDR)) {
		RJTLogger_print("DFU: start, no memory");
	
		return RJT_USB_ERROR_NO_MEMORY;
	}
	
	RJTLogger_print("DFU: Expected CRC: %x", cmd.crc);

	// Reset cache
	reset_dfu_context(&mDFU);

	// Erase application memory
	erase_application();

	return RJT_USB_ERROR_NONE;
}


enum RJT_USB_ERROR RJTUSBBridgeDFU_writeData(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	//RJTLogger_print("DFU: write data");

	bool success = enqueue_dfu_data(&mDFU, cmd_data, cmd_len);

	if(false == success) {
		return RJT_USB_ERROR_NO_MEMORY;
	}

	ASSERT(*rsp_len >= sizeof(mDFU.current_write_addr));
	memcpy(rsp_data, &mDFU.current_write_addr, sizeof(mDFU.current_write_addr));

	*rsp_len = sizeof(mDFU.current_write_addr);

	return RJT_USB_ERROR_NONE;
}


enum RJT_USB_ERROR RJTUSBBridgeDFU_readData(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	RJT_USB_BRIDGE_BEGIN_CMD
		uint8_t  len;
	RJT_USB_BRIDGE_END_CMD

	cmd.len = MIN(cmd.len, *rsp_len);
	//RJTLogger_print("DFU: cmd.len: %d", cmd.len);

	size_t num2read = MIN(SK_NVM_SIZE_BYTES, mDFU.current_read_addr + cmd.len) - mDFU.current_read_addr;
	//RJTLogger_print("DFU: num2read: %d", num2read);

	memcpy(rsp_data, (uint8_t *)mDFU.current_read_addr, num2read);

	*rsp_len = num2read;

	mDFU.current_read_addr += num2read;

	return RJT_USB_ERROR_NONE;
}


enum RJT_USB_ERROR RJTUSBBridgeDFU_resetReadPtr(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	RJTLogger_print("DFU: reset read ptr");
	mDFU.current_read_addr = SK_APP_ADDR;
	return RJT_USB_ERROR_NONE;	
}


enum RJT_USB_ERROR RJTUSBBridgeDFU_doneWriting(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	RJTLogger_print("DFU: done");

	finalize_dfu(&mDFU);

	return RJT_USB_ERROR_NONE;
}


static void write_test_data(void)
{
	uint8_t buf[NVMCTRL_PAGE_SIZE];

	for(size_t k = 0; k < sizeof(buf); k++) {
		buf[k] = k & 0xff;
	}

	size_t app_size = SK_NVM_SIZE_BYTES - SK_APP_ADDR;
	size_t data_written = 0;

	struct DFUContext dfu;

	reset_dfu_context(&dfu);

	RJTLogger_print("Writing test data...");
	RJTLogger_process();

	while(data_written < app_size) {
	
		//RJTLogger_print("Enqueuing... so far: %d", data_written);
		//RJTLogger_process();

		enqueue_dfu_data(&dfu, buf, sizeof(buf));

		#if 0
		enum status_code status;
		do {
			status = nvm_write_buffer(
				data_written + SK_APP_HEADER_ADDR, buf, NVMCTRL_PAGE_SIZE);
		} while(status == STATUS_BUSY);
		#endif

		data_written += sizeof(buf);
	}

	finalize_dfu(&dfu);

	RJTLogger_print("Done writing test data!");
	RJTLogger_process();
}


void RJTUSBBridgeDFU_init(void)
{
	
	struct nvm_config config;

	nvm_get_config_defaults(&config);

	config.manual_page_write = false;

	nvm_set_config(&config);

	//erase_application();
	//write_test_data();

	reset_dfu_context(&mDFU);
}



#pragma GCC pop_options