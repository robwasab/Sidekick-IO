/*
 * rjt_logger.h
 *
 * Created: 10/18/2020 2:16:47 PM
 *  Author: robbytong
 */ 


#ifndef RJT_LOGGER_H_
#define RJT_LOGGER_H_

void RJTLogger_process(void);

void RJTLogger_print(const char fmt[], ...);

void RJTLogger_init(void);


#endif /* RJT_LOGGER_H_ */