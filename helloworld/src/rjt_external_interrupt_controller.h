/*
 * rjt_external_interrupt_controller.h
 *
 * Created: 11/28/2020 11:59:54 PM
 *  Author: robbytong
 */ 


#ifndef RJT_EXTERNAL_INTERRUPT_CONTROLLER_H_
#define RJT_EXTERNAL_INTERRUPT_CONTROLLER_H_

#include <stdint.h>
#include <stdbool.h>
#include <asf.h>

typedef void (*RJTEICCallback_t)(void * self, uint8_t pinno, uint8_t intno);

struct RJTEIC {
	bool module_init;
	RJTEICCallback_t callbacks[EIC_NUMBER_OF_INTERRUPTS];
	uint8_t intno2gpio[EIC_NUMBER_OF_INTERRUPTS];
};

typedef struct RJTEIC RJTEIC_t;

void RJTEIC_init(RJTEIC_t * self);


enum RJT_EIC_EXT_INT
{
	RJT_EIC_EXT_INT0 = 0,
	RJT_EIC_EXT_INT1,
	RJT_EIC_EXT_INT2,
	RJT_EIC_EXT_INT3,
	RJT_EIC_EXT_INT4,
	RJT_EIC_EXT_INT5,
	RJT_EIC_EXT_INT6,
	RJT_EIC_EXT_INT7,
	RJT_EIC_EXT_INT8,
	RJT_EIC_EXT_INT9,
	RJT_EIC_EXT_INT10,
	RJT_EIC_EXT_INT11,
	RJT_EIC_EXT_INT12,
	RJT_EIC_EXT_INT13,
	RJT_EIC_EXT_INT14,
	RJT_EIC_EXT_INT15,
};

enum RJT_EIC_DETECTION
{
	RJT_EIC_DETECTION_NONE = EIC_CONFIG_SENSE0_NONE_Val,
	RJT_EIC_DETECTION_RISE = EIC_CONFIG_SENSE0_RISE_Val,
	RJT_EIC_DETECTION_FALL = EIC_CONFIG_SENSE0_FALL_Val,
	RJT_EIC_DETECTION_BOTH = EIC_CONFIG_SENSE0_BOTH_Val,
	RJT_EIC_DETECTION_HIGH = EIC_CONFIG_SENSE0_HIGH_Val,
	RJT_EIC_DETECTION_LOW  = EIC_CONFIG_SENSE0_LOW_Val,
};

struct RJTEICConfig
{
	enum RJT_EIC_EXT_INT ext_int_sel;
	enum RJT_EIC_DETECTION eic_detection; 
	uint8_t gpio;
	uint8_t gpio_mux_position;
	RJTEICCallback_t callback;
};

typedef struct RJTEICConfig RJTEICConfig_t;

void RJTEIC_configure(RJTEIC_t * self, RJTEICConfig_t * config);

void RJTEIC_enableInterrupt(enum RJT_EIC_EXT_INT ext_intno);

void RJTEIC_disableInterrupt(enum RJT_EIC_EXT_INT ext_intno);


#endif /* RJT_EXTERNAL_INTERRUPT_CONTROLLER_H_ */