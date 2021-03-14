#include <asf.h>
#include <port.h>

#include "rjt_logger.h"
#include "rjt_queue.h"
#include "rjt_uart.h"
#include "rjt_usb_bridge.h"
#include "rjt_clock_logger.h"
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

#include "bootloader.h"


static bool run_application(void)
{
	union SKAppHeader * app_header = (union SKAppHeader *) (SK_APP_ADDR + sizeof(DeviceVectors));

	if(SK_IMAGE_MAGIC == app_header->fields.magic)
	{
		RJTLogger_print("application present!");
		
		#define PRINT_FIELD(field, fmt)	RJTLogger_print(#field ": " fmt, app_header->fields.field); RJTLogger_process()
		
		PRINT_FIELD(header_version, "%x");
		PRINT_FIELD(fw_major_version, "%x");
		PRINT_FIELD(fw_minor_version, "%x");
		PRINT_FIELD(fw_patch_version, "%x");
		PRINT_FIELD(vector_table_addr, "%x");

		// Flush
		RJTLogger_process();

		// Remap the vector table offset
		uint32_t * vector_table_addr = 
			(uint32_t *) SK_APP_ADDR; 
		
		SCB->VTOR = (uint32_t) vector_table_addr;

		RJTLogger_print("vector table addr: %x, %b", vector_table_addr, vector_table_addr);
		RJTLogger_print("app stack addr: %x", vector_table_addr[0]);
		RJTLogger_print("app reset addr: %x", vector_table_addr[1]);
		
		RJTLogger_process();

		RJTLogger_deinit();

		__asm volatile(
			"msr msp, %0 \n"	/* copy reg 0 into stack pointer */
			"bx %1       \n"	/* branch to address in reg 1 */
			:	/* no outputs */
			: "r"(vector_table_addr[0]), "r"(vector_table_addr[1])
			: /* clobbered registers */
		);

		return true;
	}
	else {
		RJTLogger_print("application not present...");
		return false;
	}
}

#define RESET_SHARED_MEMORY(msg)	\
do { \
	RJTLogger_print("Zeroing shared memory: %s", msg); \
	memset((void *)SK_SHARED_MEMORY_ADDR, 0, SK_SHARED_MEMORY_SIZE); \
} while(0)


static void init_shared_memory(void)
{
	// If reset was not user reset, reset the shared memory
	if(false == PM->RCAUSE.bit.SYST)
	{
		RESET_SHARED_MEMORY("Not user reset");
	}
	else {
		// preserve the shared memory
		if(SK_SHARED_MEMORY_OBJ->fw_mode >= RJT_USB_BRIDGE_MODE_MAX) {
			RESET_SHARED_MEMORY("Invalid fw_mode");
		}
	}
}


static void init_dfu_sense_pin(void)
{
	struct port_config config;
	port_get_config_defaults(&config);

	config.direction = PORT_PIN_DIR_INPUT;
	config.input_pull = PORT_PIN_PULL_UP;

	port_pin_set_config(PIN_PB15, &config);
}


static void init_led(void)
{
	struct port_config config;
	port_get_config_defaults(&config);

	config.direction = PORT_PIN_DIR_OUTPUT;
	config.input_pull = PORT_PIN_PULL_NONE;

	port_pin_set_config(PIN_PB30, &config);
}


static bool dfu_pin_level(void)
{
	return port_pin_get_input_level(PIN_PB15);
}


static void sof_callback(void)
{
	static uint32_t count = 0;
	static bool level = false;

	if(count >= 1000) {
		count = 0;
		//RJTLogger_print("dfu sense pin status: %?", dfu_pin_level());
		port_pin_set_output_level(PIN_PB30, level);
		level ^= true;
	}
	else {
		count++;
	}
}

SOF_REGISTER_CALLBACK(sof_callback);


int main (void)
{
	system_init();

	init_led();
	init_dfu_sense_pin();

	RJTLogger_init();

	RJTLogger_print("Bootloader Initialized");

	init_shared_memory();

	RJTLogger_process();

	if(true == dfu_pin_level()) 
	{
		// dfu pin is not asserted, continue checking if 
		// app is present

		if(RJT_USB_BRIDGE_MODE_APP == SK_SHARED_MEMORY_OBJ->fw_mode) 
		{
			run_application();
		}
		else {
			RJTLogger_print("Forced into DFU mode...");
			// reset the firmware mode
			SK_SHARED_MEMORY_OBJ->fw_mode = RJT_USB_BRIDGE_MODE_APP;
		}
	}
	else {
			RJTLogger_print("DFU pin held low...");
	}

	// uncomment to print debug info about the clock system
	//RJTClockLogger_gclk();
	//RJTClockLogger_powerManager();

	RJTUSBBridge_init();

	udc_start();

	system_interrupt_enable_global();

	/* Insert application code here, after the board has been initialized. */

	/* This skeleton code simply sets the LED to the state of the button. */
	while (1) {

		RJTLogger_process();
	}
}
