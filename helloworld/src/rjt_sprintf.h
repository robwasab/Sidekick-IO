/*
 * rjt_sprintf.h
 *
 * Created: 10/18/2020 2:08:44 PM
 *  Author: robbytong
 */ 


#ifndef RJT_SPRINTF_H_
#define RJT_SPRINTF_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>


uint8_t RJTSprintf_fromArgList(
	uint8_t  buf[],
	size_t  buf_size,
	const char  fmt[],
	va_list  argp);


uint8_t RJTSprintf(
	uint8_t  buf[],
	size_t  buf_size,
	const char  fmt[],
	...);

#endif /* RJT_SPRINTF_H_ */