/*************************************************************
 * COURSE WARE ver. 2.0
 * 
 * Permitted to use for educational and research purposes only.
 * NO WARRANTY.
 *
 * Faculty of Information Technology
 * Czech Technical University in Prague
 * Author: Miroslav Skrbek (C)2010,2011,2012
 *         skrbek@fit.cvut.cz
 * 
 **************************************************************
 */
#ifndef __LOG_H
#define __LOG_H

#define _ADDED_C_LIB 1
#include <stdio.h>
#include "timer.h"

#define NL "\n"

#define __MSG_BUF_SIZE 64

#define PRINT(str) { log_str(str NL);}
#define PRINTF(a,args...) { snprintf(__msg_buf, __MSG_BUF_SIZE, a, ## args); log_str(__msg_buf);}

//#define PRINT(str)
//#define PRINTF(a,args...)

extern char __msg_buf[__MSG_BUF_SIZE];
extern void log_init();

extern void log_str(char* s);
extern void log_int(char* s, long v);

extern void log_main_loop();

#endif

