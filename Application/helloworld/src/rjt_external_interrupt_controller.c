/*
 * rjt_external_interrupt_controller.c
 *
 * Created: 11/28/2020 11:16:27 PM
 *  Author: robbytong
 */ 

#include <asf.h>
#include "utils.h"
#include "rjt_logger.h"
#include "rjt_external_interrupt_controller.h"
#include <pinmux.h>

static RJTEIC_t * mSelf = NULL;

// some idiot thought it would be a good idea to 
// use the name enable in compiler.h
#undef ENABLE

#define ENABLE_EIC() \
do {\
	EIC->CTRL.bit.ENABLE = 1;\
	while(EIC->STATUS.bit.SYNCBUSY);\
} while(0)

#define DISABLE_EIC()\
do {\
	EIC->CTRL.bit.ENABLE = 0;\
	while(EIC->STATUS.bit.SYNCBUSY);\
} while(0)


void RJTEIC_configure(RJTEIC_t * self, RJTEICConfig_t * eic_config)
{
	ASSERT(NULL != self);
	ASSERT(NULL != eic_config);
	ASSERT(NULL != eic_config->callback);
	ASSERT(true == self->module_init);
	ASSERT(eic_config->ext_int_sel < EIC_NUMBER_OF_INTERRUPTS);

	system_interrupt_enter_critical_section();

	DISABLE_EIC();

	uint8_t ext_intno = (uint8_t) eic_config->ext_int_sel;

	// Disable interrupt
	EIC->INTENCLR.reg = (1 << ext_intno);

	// Clear interrupt
	EIC->INTFLAG.reg = (1 << ext_intno);
	RJTLogger_print("config ext int: %d", ext_intno);

	uint8_t config_offset = ext_intno / 8;

	// Enables the filter
	uint8_t config_pos = 4 * (ext_intno % 8);
	
	// clear the configuration settings
	EIC->CONFIG[config_offset].reg &= 
			~((EIC_CONFIG_SENSE0_Msk | EIC_CONFIG_FILTEN0) << config_pos);
	
	uint32_t new_config = EIC_CONFIG_FILTEN0 | (eic_config->eic_detection << EIC_CONFIG_SENSE0_Pos);

	EIC->CONFIG[config_offset].reg |= new_config << config_pos;

	// Interrupts
	self->callbacks[ext_intno] = eic_config->callback;
	self->intno2gpio[ext_intno] = eic_config->gpio;

	NVIC_SetPriority(EIC_IRQn, APP_LOW_PRIORITY);
	NVIC_EnableIRQ(EIC_IRQn);

	struct system_pinmux_config config;

	system_pinmux_get_config_defaults(&config);

	config.direction = SYSTEM_PINMUX_PIN_DIR_INPUT;
	config.input_pull = SYSTEM_PINMUX_PIN_PULL_UP;
	config.powersave = false;
	config.mux_position = eic_config->gpio_mux_position;

	system_pinmux_pin_set_config(eic_config->gpio, &config);

	ENABLE_EIC();
	
	system_interrupt_leave_critical_section();

	//RJTLogger_print("configured eic!");
}


void EIC_Handler(void)
{
	system_interrupt_enter_critical_section();

	if(NULL == mSelf) {
		//RJTLogger_print("EIC Callback: EIC is NULL...");
		EIC->INTFLAG.reg = EIC->INTFLAG.reg;
		return;
	}

	//RJTLogger_print("EIC Handler: %x", EIC->INTFLAG);

	uint8_t k;
	for(k = 0; k < EIC_NUMBER_OF_INTERRUPTS; k++)
	{
		if(EIC->INTFLAG.reg & (1 << k))
		{
			ASSERT(NULL != mSelf->callbacks[k]);

			mSelf->callbacks[k](mSelf, mSelf->intno2gpio[k], k);
			EIC->INTFLAG.reg = (1 << k);
		}
	}

	//EIC->INTFLAG.reg = EIC->INTFLAG.reg;

	system_interrupt_leave_critical_section();
}


void RJTEIC_enableInterrupt(enum RJT_EIC_EXT_INT ext_intno)
{
	// Enable interupt
	EIC->INTENSET.reg = (1 << ext_intno);	
};


void RJTEIC_disableInterrupt(enum RJT_EIC_EXT_INT ext_intno)
{
	// Disable interrupt
	EIC->INTENCLR.reg = (1 << ext_intno);
};



void RJTEIC_init(RJTEIC_t * self)
{
	ASSERT(NULL != self);
	ASSERT(false == self->module_init);

	struct system_gclk_chan_config config;
	system_gclk_chan_get_config_defaults(&config);
	config.source_generator = GCLK_GENERATOR_3;

	system_gclk_chan_set_config(GCLK_CLKCTRL_ID_EIC, &config);

	system_gclk_chan_enable(GCLK_CLKCTRL_ID_EIC);

	
	ENABLE_EIC();	
	while(EIC->STATUS.bit.SYNCBUSY);

	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | GCLK_CLKCTRL_ID_EIC;

	EIC->CTRL.bit.SWRST = 1;

	while(GCLK->STATUS.bit.SYNCBUSY);

	self->module_init = true;

	mSelf = self;
}