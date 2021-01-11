#include <asf.h>


#include "rjt_logger.h"
#include "rjt_queue.h"
#include "rjt_uart.h"

#include "utils.h"


uint32_t get_nvic_pending(void)
{
	return __get_IPSR();
}


__PACKED_STRUCT SavedContext {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;								// link register of saved context (before interrupt)
	uint32_t return_address;		// return address to go back to the saved context
	uint32_t xpsr;
};


#pragma GCC push_options
#pragma GCC optimize("O0")

static void my_fault_handler(uint32_t stack_pointer)
{
	struct SavedContext * saved_context =
	(struct SavedContext *) stack_pointer;

	uint32_t pending_nvic = get_nvic_pending();

	__ASM("BKPT");
	while(1) {

	}
}


#define HARDFAULT_HANDLING_ASM()\
__asm volatile(					\
		"mov r0, #4 \n"			\
		"mov r1, lr \n"			\
		"tst r1, r0 \n"			\
		"beq 1f \n"					\
		"mrs r0, psp \n"		\
		"b my_fault_handler \n"	\
		"1:"										\
		"mrs r0, msp \n"				\
		"b my_fault_handler \n"	\
)


__attribute__((naked)) void HardFault_Handler(void)
{
	HARDFAULT_HANDLING_ASM();
}

#pragma GCC pop_options



int main (void)
{
	system_init();

	RJTLogger_init();

	RJTLogger_print("Bootloader Initialized");
	RJTLogger_process();

	udc_start();

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

		RJTLogger_process();
	}
}
