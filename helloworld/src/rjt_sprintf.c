/*
 * rjt_sprintf.c
 *
 * Created: 10/18/2020 1:57:49 PM
 *  Author: robbytong
 */ 

#include "utils.h"
#include "rjt_sprintf.h"
#include <string.h>

static char nibble2hex(uint8_t nib)
{
	nib &= 0x0f;
	return nib < 10 ? nib + '0' : (nib-10) + 'A';
}


static void byte2hex(uint8_t val, char msg[2])
{
	msg[0] = nibble2hex( (val & 0xf0) >> 4 );
	msg[1] = nibble2hex( (val & 0x0f) );
}


static uint8_t word2hex(uint32_t val, char msg[10], bool leading_0x)
{
	uint8_t k;
	uint8_t chars_written = 0;
	
	if(leading_0x)
	{
		msg[chars_written++] = '0';
		msg[chars_written++] = 'x';
	}
	
	bool non_zero_encountered = false;

	for(k = 0; k < 4; k++)
	{
		uint8_t offset = (3 - k) * 8;
		uint32_t mask = (0xff << offset);
		uint8_t byte = (val & mask) >> offset;
		
		if(0 < byte) {
			non_zero_encountered = true;
		}

		if(offset > 0) {
			if(false == non_zero_encountered && byte == 0) {
				continue;
			}
		}
		byte2hex(byte, &msg[chars_written]);
		chars_written += 2;
	}
	return chars_written;
}


static uint8_t calculate_digits(uint32_t val)
{
	uint32_t order = 1;
	uint32_t magnitude = 10;

	// Maximum size we can print is a 32 bit number aka 0xffffffff.
	// 0xffffffff -> 10^(9.63...)
	// We'll count up to 9, and if the num is still bigger than that we'll return
	// 10 digits
	while(true)
	{
		if(val < magnitude)
		break;
		
		order += 1;

		if(10 <= order)
		break;

		magnitude *= 10;
	}

	return order;
}


static uint8_t int2ascii(uint32_t val, char * buf)
{
	uint8_t digit = val % 10;
	val /= 10;

	if(0 < val)
	{
		uint8_t offset = int2ascii(val, buf);

		buf[offset] = '0' + digit;
		return 1 + offset;
	}
	else
	{
		buf[0] = '0' + digit;
		return 1;
	}
}

uint8_t RJTSprintf_fromArgList(
	uint8_t  buf[],
	size_t  buf_size,
	const char  fmt[],
	va_list  argp)
{
	uint8_t buf_offset = 0;

	while( (*fmt != '\0') && (buf_offset < buf_size - 1) )
	{
		if(*fmt == '%')
		{
			fmt++;
			uint32_t next_arg = va_arg(argp, uint32_t);
			
			bool leading_0x = true;
			
			switch(*fmt)
			{
				case 's': {
					uint8_t len = strlen((char *) next_arg);
					
					memcpy(&buf[buf_offset],
					(char *)next_arg,
					MIN(buf_size - buf_offset, len));
					buf_offset += len;
				} break;
				
				case 'c':
				if(buf_size - buf_offset >= 1)
				{
					buf[buf_offset++] = (char) next_arg;
				}
				break;

				// Format as hexidecimal
				case 'X':
				leading_0x = false;
				case 'x': {
					// Write the numbers in hex as XXXXXXXXH = 9 characters worst case
					if(buf_size - buf_offset >= 10)
					{
						uint8_t offset;
						offset = word2hex(next_arg, (char *) &buf[buf_offset], leading_0x);
						buf_offset += offset;
					}
				} break;

				// Boolean
				case '?': {
					const char true_str[] = "true";
					const char false_str[] = "false";
					
					if(next_arg & 0x01) {
						// True
						if(buf_size - buf_offset >= 4) {
							memcpy(&buf[buf_offset], true_str, 4);
							buf_offset += 4;
						}
					}
					else {
						if(buf_size - buf_offset >= 5) {
							memcpy(&buf[buf_offset], false_str, 5);
							buf_offset += 5;
						}
					}
				} break;

				// Format as binary number
				case 'b':
				case 'B': {
					// Calculate how many characters this is going to take
					int32_t k;

					// Start from msb to eliminate any leading zeros
					for(k = 31; k > 7; --k) {
						if(next_arg & (1 << k)) {
							break;
						}
					}
					
					// k + 1 is the size of this binary number
					if(buf_size - buf_offset >= (uint32_t)(1+k))
					{
						for(; k >= 0; --k) {
							buf[buf_offset++] = next_arg & (1 << k) ? '1' : '0';
						}
					}
				} break;

				// Format as decimal
				case 'd': {
					char sign = '+';
					if(next_arg > (1u << 31))
					{
						// Dealing with a negative number...
						sign = '-';
						// Make it positive...
						next_arg = ~next_arg + 1;
					}
					uint8_t num_digits = calculate_digits(next_arg);
					
					// Add 1 for the sign
					num_digits += 1;

					if(buf_size - buf_offset >= num_digits)
					{
						// Add the sign
						buf[buf_offset++] = sign;

						uint8_t offset;
						offset = int2ascii(next_arg, (char *) &buf[buf_offset]);
						buf_offset += offset;
					}
				} break;
			}
		}
		else
		{
			buf[buf_offset++] = *fmt;
		}
		fmt++;
	}

	ASSERT(buf_offset < buf_size);

	buf[buf_offset++] = '\0';
	
	return buf_offset;
}


uint8_t RJTSprintf(
	uint8_t  buf[],
	size_t  buf_size,
	const char  fmt[],
	...)
{
	va_list argp;
	va_start(argp, fmt);

	uint8_t buf_offset = 0;

	buf_offset = 
		RJTSprintf_fromArgList(buf, buf_size, fmt, argp);

	va_end(argp);
	
	return buf_offset;
}