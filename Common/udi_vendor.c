
#include "rjt_logger.h"
#include "rjt_usb_bridge.h"

#include "conf_usb.h"
#include "usb_protocol.h"
#include "udd.h"
#include "udc.h"
#include "udi_vendor.h"
#include "utils.h"

#include <string.h>
#include <asf.h>

#if 1
#define CRITICAL_SECTION_ENTER()							\
do {																					\
	system_interrupt_enter_critical_section()

#define CRITICAL_SECTION_EXIT()								\
	system_interrupt_leave_critical_section();	\
} while(0)

#else

#define CRITICAL_SECTION_ENTER()

#define CRITICAL_SECTION_EXIT()

#endif

static COMPILER_WORD_ALIGNED uint8_t mWriteBuf[UDI_VENDOR_EP_SIZE];

static COMPILER_WORD_ALIGNED uint8_t mReadBuf[UDI_VENDOR_EP_SIZE];

static COMPILER_WORD_ALIGNED uint32_t mNotify;

static size_t mReadBufLen = 0;

static bool mReadEPEnabled = false;
static bool mWriteEPEnabled = false;
static bool mNotifyEPEnabled = false;
static bool mNotifyEPAborting = false;

static void read_transfer_callback(udd_ep_status_t  status,
	iram_size_t  nb_transfered,	udd_ep_id_t  ep)
{
	//RJTLogger_print("read_transfer_callback");
	CRITICAL_SECTION_ENTER();

	switch(status)
	{
		case UDD_EP_TRANSFER_ABORT:
			//RJTLogger_print("abort read success");

			if(true == mReadEPEnabled)
			{
				//RJTLogger_print("starting read endpoint");

				bool short_packet = mReadBufLen < UDI_VENDOR_EP_SIZE;

				// prepare a read job
				bool success =
				udd_ep_run(UDI_VENDOR_EP_READ_ADDR, short_packet, mReadBuf, mReadBufLen, read_transfer_callback);
				ASSERT(success);
			}
			else {
				RJTLogger_print("not starting read endpoint...");
			}
			break;

		case UDD_EP_TRANSFER_OK:
			mReadEPEnabled = false;
			//RJTLogger_print("read success");
			RJTUSBBridge_rspSent();
			break;
	}

	CRITICAL_SECTION_EXIT();
}


// prototype
static void write_transfer_callback(udd_ep_status_t  status, iram_size_t  nb_transfered, udd_ep_id_t  ep);


static void process_write(iram_size_t nb_transfered)
{
	CRITICAL_SECTION_ENTER();

	//RJTLogger_print("write transfer: %d", nb_transfered);

	uint8_t buf[sizeof(mReadBuf)];
	size_t buflen = sizeof(buf);

	bool send_cached_rsp = true;

	RJTUSBBridge_processCmd(mWriteBuf, nb_transfered, &send_cached_rsp, buf, &buflen);

	if(false == send_cached_rsp) {
		// copy the local contents to the read buffer
		memset(mReadBuf, 0, sizeof(mReadBuf));

		memcpy(mReadBuf, buf, buflen);
		mReadBufLen = buflen;

		//RJTLogger_print("read transfer: %d", mReadBufLen);
	}

	//RJTLogger_print("starting write endpoint");

	// re-initialize write transfer
	bool success =
	udd_ep_run(UDI_VENDOR_EP_WRITE_ADDR, true, mWriteBuf, sizeof(mWriteBuf), write_transfer_callback);
	ASSERT(success);

	if(true == mReadEPEnabled)
	{
		RJTLogger_print("aborting read endpoint");

		udd_ep_abort(UDI_VENDOR_EP_READ_ADDR);
	}
	else
	{
		// prepare a read job
		//RJTLogger_print("starting read endpoint");

		bool short_packet = mReadBufLen < UDI_VENDOR_EP_SIZE;

		//RJTLogger_print("short packet: %?", short_packet);

		success =
		udd_ep_run(UDI_VENDOR_EP_READ_ADDR, short_packet, mReadBuf, mReadBufLen, read_transfer_callback);
		ASSERT(success);

		mReadEPEnabled = true;
	}

	CRITICAL_SECTION_EXIT();
}


static void write_transfer_callback(udd_ep_status_t  status,
	iram_size_t  nb_transfered, udd_ep_id_t  ep)
{
	//RJTLogger_print("nb_transfered: %d", nb_transfered);

	if(UDI_VENDOR_EP_WRITE_ADDR != ep) {
		RJTLogger_print("invalid write address: %x", ep);
		return;
	}

	CRITICAL_SECTION_ENTER();

	switch(status)
	{
		case UDD_EP_TRANSFER_OK: {
			process_write(nb_transfered);
		} break;

		case UDD_EP_TRANSFER_ABORT: {
			RJTLogger_print("write abort success");
			if(true == mWriteEPEnabled) 
			{
				RJTLogger_print("starting write endpoint");

				bool success =
				udd_ep_run(UDI_VENDOR_EP_WRITE_ADDR, 
						true, mWriteBuf, sizeof(mWriteBuf), write_transfer_callback);
				
				ASSERT(success);
			}
			else {
				RJTLogger_print("not starting write endpoint...");
			}
		} break;
	}

	CRITICAL_SECTION_EXIT();
}


static void interrupt_transfer_callback(udd_ep_status_t status,
		iram_size_t nb_transfered, udd_ep_id_t ep)
{
	if(UDI_VENDOR_EP_NOTIFY_ADDR != ep) {
		RJTLogger_print("invalid notify address: %x", ep);
		return;
	}

	CRITICAL_SECTION_ENTER();

	switch(status)
	{
		case UDD_EP_TRANSFER_OK: {
			//RJTLogger_print("interrupt transfer ok!");

			bool success =
				udd_ep_run(UDI_VENDOR_EP_NOTIFY_ADDR,
					false, (uint8_t *)&mNotify, sizeof(mNotify), interrupt_transfer_callback);
			ASSERT(success);
		
		} break;

		case UDD_EP_TRANSFER_ABORT: {
			RJTLogger_print("interrupt transfer aborted...");
			
			mNotifyEPAborting = false;

			if(true == mNotifyEPEnabled)
			{
				bool success =
					udd_ep_run(UDI_VENDOR_EP_NOTIFY_ADDR,
						false, (uint8_t *)&mNotify, sizeof(mNotify), interrupt_transfer_callback);
				ASSERT(success);
			}
		} break;
	}

	CRITICAL_SECTION_EXIT();
}


static bool udi_vendor_enable(void)
{
	RJTLogger_print("vendor enable");
	RJTLogger_print("sof callback len: %d", SOF_CALLBACK_LEN);

	CRITICAL_SECTION_ENTER();

	if(false == mWriteEPEnabled)
	{
		bool success = 
			udd_ep_run(UDI_VENDOR_EP_WRITE_ADDR, 
				true, mWriteBuf, sizeof(mWriteBuf), write_transfer_callback);
		ASSERT(success);
	}
	else {
		udd_ep_abort(UDI_VENDOR_EP_WRITE_ADDR);
	}

	mWriteEPEnabled = true;

	if(false == mNotifyEPEnabled)
	{
		bool success = 
			udd_ep_run(UDI_VENDOR_EP_NOTIFY_ADDR, 
				false, (uint8_t *)&mNotify, sizeof(mNotify), interrupt_transfer_callback);
		ASSERT(success);
	}
	else {
		mNotifyEPAborting = true;
		udd_ep_abort(UDI_VENDOR_EP_NOTIFY_ADDR);
	}

	mNotifyEPEnabled = true;

	CRITICAL_SECTION_EXIT();

	return true;
}


void RJTUSBBridge_setInterruptStatus(uint32_t interrupt_status)
{
	CRITICAL_SECTION_ENTER();

	if(mNotifyEPEnabled) 
	{
		if(false == mNotifyEPAborting)
		{
			mNotifyEPAborting = true;
			
			udd_ep_abort(UDI_VENDOR_EP_NOTIFY_ADDR);
			
			mNotify = interrupt_status;
		}
		else {
			// already aborting, nothing to do
			mNotify = interrupt_status;
		}
	}

	CRITICAL_SECTION_EXIT();
}


static void udi_vendor_disable(void)
{
	RJTLogger_print("vendor disable");
}


static bool udi_vendor_setup(void)
{
	RJTLogger_print("vendor setup");

	bool ret_code = false;

	CRITICAL_SECTION_ENTER();

	// Parse the udd_g_ctrlreq to determine the command
	if(Udd_setup_is_out())
	{
		// host -> function transfer

		if(USB_REQ_TYPE_VENDOR == Udd_setup_type())
		{
			// vendor request

			if(USB_REQ_RECIP_INTERFACE == Udd_setup_recipient())
			{
				// interface recipient
				//RJTLogger_print("bRequest: %x", udd_g_ctrlreq.req.bRequest);
				//RJTLogger_print("  wValue: %x", udd_g_ctrlreq.req.wValue);
				//RJTLogger_print("  wIndex: %x", udd_g_ctrlreq.req.wIndex);

				ret_code = RJTUSBBridge_processControlRequestWrite(
					udd_g_ctrlreq.req.bmRequestType,
					udd_g_ctrlreq.req.bRequest,
					udd_g_ctrlreq.req.wValue,
					&udd_g_ctrlreq.payload,
					&udd_g_ctrlreq.payload_size);
			}
			else
			{
				// not yet supported recipient
				RJTLogger_print("unsupported recipient type");
			}
		}
		else
		{
			// not yet supported request type
			RJTLogger_print("unsupported request type");
		}
	}
	else if(Udd_setup_is_in())
	{
		// function -> host transfer

		if(USB_REQ_TYPE_VENDOR == Udd_setup_type()) 
		{
			if(USB_REQ_RECIP_INTERFACE == Udd_setup_recipient()) 
			{
				ret_code = RJTUSBBridge_processControlRequestRead(
					udd_g_ctrlreq.req.bmRequestType,
					udd_g_ctrlreq.req.bRequest,
					udd_g_ctrlreq.req.wValue,
					&udd_g_ctrlreq.payload,
					&udd_g_ctrlreq.payload_size);
			}
			else {
				// not yet supported recipient
				RJTLogger_print("IN: unsupported recipient type");	
			}
		}
		else {
			// not yet supported request type
			RJTLogger_print("IN: unsupported request type");
		}
	}

	CRITICAL_SECTION_EXIT();

	return ret_code;
}

static uint8_t udi_vendor_getsetting(void)
{
	RJTLogger_print("vendor get setting...");
	return 0;
}

static void udi_vendor_sof_notify(void)
{
	for(size_t k = 0; k < SOF_CALLBACK_LEN; k++) {
		SOF_CALLBACK_ARR[k]();
	}
}


UDC_DESC_STORAGE udi_api_t udi_api_vendor = {
	.enable = udi_vendor_enable,
  .disable = udi_vendor_disable,
  .setup = udi_vendor_setup,
  .getsetting = udi_vendor_getsetting,
  .sof_notify = udi_vendor_sof_notify,
};
