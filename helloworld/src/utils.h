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

#endif /* UTILS_H_ */