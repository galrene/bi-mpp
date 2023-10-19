#include <time.h>
#include "utils.h"

double get_time() {
	double t = (1.0 * clock()) / CLOCKS_PER_SEC;
	return t;
}

void wait_for(double start, double delay) {
	while (get_time() - start < delay) {
		usleep(5);
	}
}	