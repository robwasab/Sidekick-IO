/*
 * rjt_usb_bridge_dfu.c
 *
 * Created: 1/31/2021 6:48:18 PM
 *  Author: robbytong
 */ 

#include "rjt_usb_bridge_app.h"
#include "rjt_logger.h"
#include "bootloader.h"
#include "utils.h"

#include <asf.h>


static bool mResetCountDownEnabled = false;
static uint32_t mResetCountDown = 0;


enum RJT_USB_ERROR RJTUSBBridgeDFU_reset(
		const uint8_t * cmd_data, size_t cmd_len, uint8_t * rsp_data, size_t * rsp_len)
{
	RJTLogger_print("DFU: reset");
	
	RJT_USB_BRIDGE_BEGIN_CMD
	uint8_t  mode;
	RJT_USB_BRIDGE_END_CMD

	if(cmd.mode >= RJT_USB_BRIDGE_MODE_MAX) {
		return RJT_USB_ERROR_PARAMETER;
	}

	SK_SHARED_MEMORY_OBJ->fw_mode = cmd.mode;

	system_interrupt_enter_critical_section();

	mResetCountDown = 1000;
	mResetCountDownEnabled = true;

	system_interrupt_leave_critical_section();

	return RJT_USB_ERROR_NONE;
}


static void my_sof_callback(void)
{
	if(mResetCountDownEnabled == true)
	{
		if(0 == mResetCountDown) {
			RJTLogger_print("reset!");
			NVIC_SystemReset();
		}
		mResetCountDown--;
	}

	/*
	static uint32_t cnt = 0;

	if(cnt == 1000) {
		//RJTLogger_print("sof callback...");
		cnt = 0;
	}
	else {
		cnt++;
	}
	*/
}


SOF_REGISTER_CALLBACK(my_sof_callback);
