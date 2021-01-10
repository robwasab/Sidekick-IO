/*
 * rjt_logger.c
 *
 * Created: 10/18/2020 2:14:28 PM
 *  Author: robbytong
 */ 

#include <stdarg.h>
#include <string.h>
#include <usart.h>
#include <asf.h>

#include "rjt_logger.h"
#include "rjt_sprintf.h"
#include "rjt_queue.h"
#include "utils.h"

static uint8_t mLogBuffer[256];
static RJTQueue mLogQueue;
static bool mDroppedMsg = false;

static struct usart_module usart_instance;

static void send_bytes(const void * data, uint8_t len)
{
	system_interrupt_enter_critical_section();

	usart_write_buffer_wait(&usart_instance, data, len);

	system_interrupt_leave_critical_section();
}

#pragma GCC push_options
#pragma GCC optimize("O0")

void RJTLogger_process(void)
{
	if(true == mDroppedMsg)
	{
		char msg[] = "***DROPPED LOG MSG***";
		
		send_bytes(msg, strlen(msg));
		mDroppedMsg = false;
	}

	uint8_t buf[64];

	while(true)
	{
		system_interrupt_enter_critical_section();

		size_t num2read = MIN(sizeof(buf), RJTQueue_getNumEnqueued(&mLogQueue));

		if(0 < num2read) {
			bool success = RJTQueue_dequeue(&mLogQueue, buf, num2read);
			ASSERT(true == success);
		}

		system_interrupt_leave_critical_section();

		if(0 == num2read) {
			break;
		}

		send_bytes(buf, num2read);
	}
}
#pragma GCC pop_options


void RJTLogger_print(const char fmt[], ...)
{
	if(true == mDroppedMsg) {
		// if we dropped a message, we can't print until we print out a notification
		// that notifies the user that the message was dropped.
		return;
	}

	va_list argp;
	va_start(argp, fmt);

	uint8_t buf[128];
	memset(buf, 0, sizeof(buf));

	size_t bufsize = sizeof(buf) - 1;

	uint8_t buf_offset = RJTSprintf_fromArgList(buf, bufsize, fmt, argp);

	va_end(argp);

	buf[buf_offset++] = '\n';

	if(RJTQueue_getSpaceAvailable(&mLogQueue) < buf_offset) {
		mDroppedMsg = true;
	}
	else {
		RJTQueue_enqueue(&mLogQueue, buf, buf_offset);
	}
}


void RJTLogger_init(void)
{
	RJTQueue_init(&mLogQueue, mLogBuffer, sizeof(mLogBuffer));

	struct usart_config config;
	usart_get_config_defaults(&config);
	
	config.baudrate = 115200;
	config.mux_setting = EDBG_CDC_SERCOM_MUX_SETTING;
	config.pinmux_pad0 = EDBG_CDC_SERCOM_PINMUX_PAD0;
	config.pinmux_pad1 = EDBG_CDC_SERCOM_PINMUX_PAD1;
	config.pinmux_pad2 = EDBG_CDC_SERCOM_PINMUX_PAD2;
	config.pinmux_pad3 = EDBG_CDC_SERCOM_PINMUX_PAD3;
	
	// Block until the uart module is ready
	while (usart_init(&usart_instance, EDBG_CDC_MODULE, &config) != STATUS_OK) { }
	
	usart_enable(&usart_instance);
}