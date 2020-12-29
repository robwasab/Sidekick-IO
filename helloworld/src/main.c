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
#include "rjt_usb_bridge.h"
#include "rjt_uart.h"

#include "utils.h"

static volatile bool cdc_enabled = false;
static uint32_t current_ticks = 0;
static uint32_t last_ticks = 0;


void SysTick_Handler(void)
{
}


void user_callback_sof_action(void)
{
	++current_ticks;	
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
	cdc_enabled = false;
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


/**
 * This could be optimized
 */
#pragma GCC push_options
#pragma GCC optimize("O0")

static void process_cdc(void)
{
	//system_interrupt_enter_critical_section();

	uint8_t buf[128];
	size_t num2read;

	// Send the UART RX Data (to the host)
	if(udi_cdc_is_tx_ready())
	{
		if(0 < RJTUart_getNumReadable())
		{
			num2read = MIN(sizeof(buf), RJTUart_getNumReadable());

			RJTUart_dequeueFromReadQueue(buf, num2read);

			// Send the data to the host
			iram_size_t numsent =
				udi_cdc_write_buf(buf, num2read);
			ASSERT(0 == numsent);
		}
	}

	// Read the data the host sent us, so we can send it (via UART tx)
	
	bool rx_ready = udi_cdc_is_rx_ready();

	if(rx_ready)
	{
		iram_size_t num_avail = udi_cdc_get_nb_received_data();

		if(0 < num_avail)
		{
			// read the received data
			num2read = MIN(num_avail, sizeof(buf));
			num2read = MIN(num2read, RJTUart_getWriteQueueAvailableSpace());

			iram_size_t num_remaining = udi_cdc_read_buf(buf, num2read);
			ASSERT(0 == num_remaining);

			RJTUart_enqueueIntoWriteQueue(buf, num2read);
		}
	}
	//system_interrupt_leave_critical_section();
}
#pragma GCC pop_options


static void log_gclk_generator(uint8_t gen_id)
{
	*((uint8_t*)&GCLK->GENCTRL.reg) = gen_id;
	
	uint8_t src    = GCLK->GENCTRL.bit.SRC;
	uint8_t genen  = GCLK->GENCTRL.bit.GENEN;
	uint8_t divsel = GCLK->GENCTRL.bit.DIVSEL;
	
	const char src2str[][64] = {
		[GCLK_GENCTRL_SRC_XOSC_Val]      = "XOSC",
		[GCLK_GENCTRL_SRC_GCLKIN_Val]    = "GCLKIN (Gen input pad)",
		[GCLK_GENCTRL_SRC_GCLKGEN1_Val]  = "GCLKGEN1",
		[GCLK_GENCTRL_SRC_OSCULP32K_Val] = "ULP32K",
		[GCLK_GENCTRL_SRC_OSC32K_Val]    = "OSC32K",
		[GCLK_GENCTRL_SRC_XOSC32K_Val]   = "XOSC32K",
		[GCLK_GENCTRL_SRC_OSC8M_Val]     = "OSC8M",
		[GCLK_GENCTRL_SRC_DFLL48M_Val]   = "DFLL48M",
		[GCLK_GENCTRL_SRC_FDPLL_Val]     = "FDPLL",
	};

	RJTLogger_print("GCLK[%d] GENCTRL.SRC   : %s", gen_id, src2str[src]);
	RJTLogger_process();
	RJTLogger_print("GCLK[%d] GENCTRL.GENEN : %?", gen_id, genen);
	RJTLogger_process();
	RJTLogger_print("GCLK[%d] GENCTRL.DIVSEL: %?", gen_id, divsel);
	RJTLogger_process();
}


static void log_gclk_peripheral_clock(uint8_t clock_id, const char * name)
{
	*((uint8_t*)&GCLK->CLKCTRL.reg) = clock_id;
	
	RJTLogger_print("GCLK Peripheral Name: %s", name);
	RJTLogger_process();
	RJTLogger_print("- GEN    : %d", GCLK->CLKCTRL.bit.GEN);
	RJTLogger_process();
	RJTLogger_print("- CLKEN  : %?", GCLK->CLKCTRL.bit.CLKEN);
	RJTLogger_process();
	RJTLogger_print("- WRTLOCK: %?", GCLK->CLKCTRL.bit.WRTLOCK);
	RJTLogger_process();
}


static void log_gclk(void)
{
	/* Print out the settings for the GCLK generators */
	uint8_t k;
	for(k = 0; k < 8; k++)
	{
		log_gclk_generator(k);
	}

	/* Print out the settings for the peripheral clocks */
	#define LOG_GCLK_PERIPH_CLOCK(clk_id) log_gclk_peripheral_clock(clk_id, #clk_id)

	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_EIC_Val);
	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_USB_Val);
	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_SERCOMX_SLOW_Val);
	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_SERCOM0_CORE_Val);
	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_SERCOM1_CORE_Val);
	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_SERCOM2_CORE_Val);
	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_SERCOM3_CORE_Val);
	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_SERCOM4_CORE_Val);
	LOG_GCLK_PERIPH_CLOCK(GCLK_CLKCTRL_ID_SERCOM5_CORE_Val);
}


static void log_power_manager(void)
{
	RJTLogger_print("PM CPU  div: %d", 1 << PM->CPUSEL.bit.CPUDIV);
	RJTLogger_print("PM APBA div: %d", 1 << PM->APBASEL.bit.APBADIV);
	RJTLogger_print("PM APBB div: %d", 1 << PM->APBBSEL.bit.APBBDIV);
	RJTLogger_print("PM APBC div: %d", 1 << PM->APBCSEL.bit.APBCDIV);
	RJTLogger_process();

	#define LOG_IF(cond) RJTLogger_print(#cond ": %?", cond); RJTLogger_process();

	RJTLogger_print("Arm High Speed Bus Masks");
	LOG_IF(PM->AHBMASK.bit.USB_);
	LOG_IF(PM->AHBMASK.bit.DMAC_);
	LOG_IF(PM->AHBMASK.bit.NVMCTRL_);
	LOG_IF(PM->AHBMASK.bit.DSU_);
	LOG_IF(PM->AHBMASK.bit.HPB2_);
	LOG_IF(PM->AHBMASK.bit.HPB1_);
	LOG_IF(PM->AHBMASK.bit.HPB0_);
	
	RJTLogger_print("\nArm Peripheral Bus A Masks");
	LOG_IF(PM->APBAMASK.bit.PAC0_);
	LOG_IF(PM->APBAMASK.bit.PM_);
	LOG_IF(PM->APBAMASK.bit.SYSCTRL_);
	LOG_IF(PM->APBAMASK.bit.GCLK_);
	LOG_IF(PM->APBAMASK.bit.WDT_);
	LOG_IF(PM->APBAMASK.bit.RTC_);
	LOG_IF(PM->APBAMASK.bit.EIC_);
	
	RJTLogger_print("\nArm Peripheral Bus B Masks");
	LOG_IF(PM->APBBMASK.bit.PAC1_);
	LOG_IF(PM->APBBMASK.bit.DSU_);
	LOG_IF(PM->APBBMASK.bit.NVMCTRL_);
	LOG_IF(PM->APBBMASK.bit.PORT_);
	LOG_IF(PM->APBBMASK.bit.DMAC_);
	LOG_IF(PM->APBBMASK.bit.USB_);
	LOG_IF(PM->APBBMASK.bit.HMATRIX_);
	
	RJTLogger_print("\nArm Peripheral Bus C Masks");
	LOG_IF(PM->APBCMASK.bit.PAC2_);
	LOG_IF(PM->APBCMASK.bit.EVSYS_);
	LOG_IF(PM->APBCMASK.bit.SERCOM0_);
	LOG_IF(PM->APBCMASK.bit.SERCOM1_);
	LOG_IF(PM->APBCMASK.bit.SERCOM2_);
	LOG_IF(PM->APBCMASK.bit.SERCOM3_);
	LOG_IF(PM->APBCMASK.bit.SERCOM4_);
	LOG_IF(PM->APBCMASK.bit.SERCOM5_);
	LOG_IF(PM->APBCMASK.bit.TCC0_);
	LOG_IF(PM->APBCMASK.bit.TCC1_);
	LOG_IF(PM->APBCMASK.bit.TCC2_);
	LOG_IF(PM->APBCMASK.bit.TC3_);
	LOG_IF(PM->APBCMASK.bit.TC4_);
	LOG_IF(PM->APBCMASK.bit.TC5_);
	LOG_IF(PM->APBCMASK.bit.TC6_);
	LOG_IF(PM->APBCMASK.bit.TC7_);
	LOG_IF(PM->APBCMASK.bit.ADC_);
	LOG_IF(PM->APBCMASK.bit.AC_);
	LOG_IF(PM->APBCMASK.bit.DAC_);
	LOG_IF(PM->APBCMASK.bit.PTC_);
	LOG_IF(PM->APBCMASK.bit.I2S_);
	LOG_IF(PM->APBCMASK.bit.AC1_);
}


#pragma GCC push_options
#pragma GCC optimize("O0")

int main (void)
{
	extern uint32_t _sstack;

	// stack watermark
	*(&_sstack) = 0xDEADBEEF;
	
	system_init();

	RJTLogger_init();

	RJTLogger_print("Logger Initialized");
	RJTLogger_process();

	// This is the UART module used by the serial bridge (not for logging)
	RJTUart_init();

	RJTUSBBridge_init();
	
	udc_start();
	
	log_power_manager();

	log_gclk();

	//configure_extint_callbacks();

	system_interrupt_enable_global();

	// Run Tests here
	//RJTQueue_test();

	// Trigger unimplemented interrupt (for testing)
	//NVIC_EnableIRQ(I2S_IRQn);
	//NVIC_SetPendingIRQ(I2S_IRQn);

	/* Insert application code here, after the board has been initialized. */

	//void(*bad_instruction)(void) = (void *) 0x20003000;

	//bad_instruction();
	
	/* This skeleton code simply sets the LED to the state of the button. */
	while (1) {
		/* Is button pressed? */
		#if 0
		if (port_pin_get_input_level(BUTTON_0_PIN) == BUTTON_0_ACTIVE) {
			/* Yes, so turn LED on. */
			port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
		} else {
			/* No, so turn LED off. */
			port_pin_set_output_level(LED_0_PIN, !LED_0_ACTIVE);
		}
		#endif
		
		if(cdc_enabled) {
			process_cdc();

			if(current_ticks - last_ticks > 500)
			{
				last_ticks = current_ticks;

				RJTUart_testTransmit();
			}
		}
	
		RJTLogger_process();
	}
}

#pragma GCC pop_options
