#ifndef __UTILS_H
#define __UTILS_H

#include <time.h>
#include <unistd.h>

#define DEBUG 0
#define DBG_MSG(...) if (DEBUG) printf(__VA_ARGS__);

#define MIN(a,b) ((a < b) ? (a) : (b))

double get_time();
void wait_for(double start, double delay);

#endif
