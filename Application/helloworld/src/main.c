
#include <asf.h>
#include <conf_usb.h>
#include <string.h>

#include "rjt_logger.h"
#include "rjt_queue.h"
#include "rjt_usb_bridge.h"
#include "rjt_uart.h"
#include "utils.h"
#include "bootloader.h"
#include "rjt_clock_logger.h"

extern uint32_t _sstack;
extern uint32_t _svector_table;
extern uint32_t _app_header_addr;

__attribute__ ((section (".app_header")))
union SKAppHeader app_header = {
	.fields = {
		.magic = SK_IMAGE_MAGIC,
		.header_version = 0x00,
		.image_type = SK_IMAGE_TYPE_APP,
		.fw_major_version = 0x00,
		.fw_minor_version = 0x00,
		.fw_patch_version = 0x00,
		.vector_table_addr = &_svector_table,
		.git_sha = {0},
	},
};


void SysTick_Handler(void)
{
}


void user_callback_sof_action(void)
{
}


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

	// From ARM Cortex M0 Generic User Guide
	// 0 = Thread mode
	// 1 = Reserved
	// 2 = NMI
	// 3 = HardFault
	// 4 - 10 = Reserved
	// 11 = SVCall
	// 14 = PendSV
	// 15 = SysTick, if implemented
	// 16 = IRQ0
	// ...
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


__attribute__((naked)) void I2S_Handler(void) 
{
	HARDFAULT_HANDLING_ASM();
}


__attribute__((naked)) void HardFault_Handler(void) 
{
	HARDFAULT_HANDLING_ASM();
}

#pragma GCC pop_options



#pragma GCC push_options
#pragma GCC optimize("O0")

int main (void)
{	
	// stack watermark
	*(&_sstack) = 0xDEADBEEF;
	
	system_interrupt_enter_critical_section();
	//__disable_irq();

	system_init();

	RJTLogger_init();

	RJTLogger_print("App Initialized");
	RJTLogger_process();

	// This is the UART module used by the serial bridge (not for logging)
	RJTUart_init();

	RJTUSBBridge_init();
	
	udc_start();
	
	//RJTClockLogger_gclk();
	//RJTClockLogger_powerManager();

	// Run Tests here
	//RJTQueue_test();

	// Trigger unimplemented interrupt (for testing)
	//NVIC_EnableIRQ(I2S_IRQn);
	//NVIC_SetPendingIRQ(I2S_IRQn);

	/* Insert application code here, after the board has been initialized. */

	//void(*bad_instruction)(void) = (void *) 0x20003000;

	//bad_instruction();
	
	//RJTLogger_print("Hello world");
	
	system_interrupt_leave_critical_section();
	//__enable_irq();

	
	/* This skeleton code simply sets the LED to the state of the button. */
	while (1) {
		/* Is button pressed? */
		
		RJTUart_processCDC();

		RJTLogger_process();
	}
}

#pragma GCC pop_options
