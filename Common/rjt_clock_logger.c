/*
 * rjt_clock_logger.c
 *
 * Created: 1/24/2021 8:59:05 PM
 *  Author: robbytong
 */ 

 #include "rjt_clock_logger.h"
 #include "rjt_logger.h"
 #include <stdint.h>
 #include <asf.h>

 static void RJTClockLogger_gclkGenerator(uint8_t gen_id)
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


 static void RJTClockLogger_gclkPeripheralClock(uint8_t clock_id, const char * name)
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


 void RJTClockLogger_gclk(void)
 {
	 /* Print out the settings for the GCLK generators */
	 uint8_t k;
	 for(k = 0; k < 8; k++)
	 {
		 RJTClockLogger_gclkGenerator(k);
	 }

	 /* Print out the settings for the peripheral clocks */
	 #define LOG_GCLK_PERIPH_CLOCK(clk_id) RJTClockLogger_gclkPeripheralClock(clk_id, #clk_id)

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


 void RJTClockLogger_powerManager(void)
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
