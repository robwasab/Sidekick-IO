/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * This is a bare minimum user application template.
 *
 * For documentation of the board, go \ref group_common_boards "here" for a link
 * to the board-specific documentation.
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# Minimal main function that starts with a call to system_init()
 * -# Basic usage of on-board LED and button
 * -# "Insert application code here" comment
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */
#include <asf.h>
#include <conf_usb.h>
#include <string.h>

#include "rjt_logger.h"
#include "rjt_queue.h"
#include "utils.h"

static volatile bool cdc_enabled = false;
static uint32_t current_ticks = 0;
static uint32_t last_ticks = 0;

static RJTQueue mTxQueue;
static uint8_t	mTxQueueBuffer[128];

void SysTick_Handler(void)
{
}


void user_callback_sof_action(void)
{
	++current_ticks;	
}


static bool enqueue_cdc_msg(void * data, size_t len)
{
	if(len <= RJTQueue_getSpaceAvailable(&mTxQueue))
	{
		bool success =
		RJTQueue_enqueue(&mTxQueue, data, len);
		ASSERT(success);
		return true;
	}
	else {
		return false;
	}
}


bool user_callback_cdc_enable(void)
{
	RJTLogger_print("cdc enabled");
	
	cdc_enabled = true;

	current_ticks = 0;
	last_ticks = 0;
	return true;
}


void user_callback_cdc_disable(uint8_t port)
{
	(void) port;
}


void user_callback_cdc_set_line_coding(uint8_t port, usb_cdc_line_coding_t * cfg)
{
	RJTLogger_print("setting line encoding...");
	RJTLogger_print("baud: %d", cfg->dwDTERate);
}


void user_callback_cdc_rx_notify(uint8_t port)
{
	RJTLogger_print("rx notify!");
}


static void process_cdc_rx_data(const uint8_t * data, size_t len)
{
	uint8_t line[17];

	const uint8_t * ptr = data;

	while(len) {
		size_t num2cpy = MIN(len, sizeof(line) - 1);

		memcpy(line, ptr, num2cpy);

		line[num2cpy] = '\0';

		ptr += num2cpy;
		len -= num2cpy;

		RJTLogger_print("%s", line);
	}
}


static void process_cdc(void)
{
	uint8_t buf[32];
	size_t num2read;

	if(0 < RJTQueue_getNumEnqueued(&mTxQueue))
	{
		
		num2read = MIN(sizeof(buf), RJTQueue_getNumEnqueued(&mTxQueue));

		bool success = 
			RJTQueue_dequeue(&mTxQueue, buf, num2read);

		ASSERT(success);

		iram_size_t numsent =
		 udi_cdc_write_buf(buf, num2read);
		ASSERT(0 == numsent);
	}

	if(udi_cdc_is_rx_ready())
	{
		iram_size_t num_avail = udi_cdc_get_nb_received_data();

		if(0 < num_avail)
		{
			// read the received data
			num2read = MIN(num_avail, sizeof(buf));

			iram_size_t num_remaining = udi_cdc_read_buf(buf, num2read);
			ASSERT(0 == num_remaining);

			process_cdc_rx_data(buf, num2read);
		}
	}
}


int main (void)
{
	RJTQueue_init(&mTxQueue, mTxQueueBuffer, sizeof(mTxQueueBuffer));

	system_init();

	RJTLogger_init();

	udc_start();
	
	RJTLogger_print("Starting program");

	// Run Tests here
	//RJTQueue_test();

	/* Insert application code here, after the board has been initialized. */

	/* This skeleton code simply sets the LED to the state of the button. */
	while (1) {
		/* Is button pressed? */
		if (port_pin_get_input_level(BUTTON_0_PIN) == BUTTON_0_ACTIVE) {
			/* Yes, so turn LED on. */
			port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
		} else {
			/* No, so turn LED off. */
			port_pin_set_output_level(LED_0_PIN, !LED_0_ACTIVE);
		}
		
		if(cdc_enabled) {
			process_cdc();

			if(current_ticks - last_ticks > 1000)
			{
				last_ticks = current_ticks;

				char msg[] = "Helloworld from CDC\n";

				RJTLogger_print("Sending hello world");
				enqueue_cdc_msg(msg, sizeof(msg));
			}
		}
	
		RJTLogger_process();
	}
}
