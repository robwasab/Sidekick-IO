/*
 * utils.h
 *
 * Created: 10/18/2020 1:59:33 PM
 *  Author: robbytong
 */ 


#ifndef UTILS_H_
#define UTILS_H_
#include <compiler.h>

#define ASSERT(x)											Assert(x)

#define MIN(a, b)                     ((a) < (b) ? (a) : (b))

#define MAX(a, b)                     ((a) < (b) ? (b) : (a))

#define ARRAY_SIZE(a)									(sizeof(a)/sizeof(a[0]))

#define APP_HIGH_PRIORITY					4
#define APP_MID_PRIORITY					5
#define APP_LOW_PRIORITY					6

#endif /* UTILS_H_ */