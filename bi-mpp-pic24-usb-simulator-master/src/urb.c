/*
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>

#include "utils.h"
#include "urb.h"
#include "sys.h"

void __urb_lock(urb_t* urb) {
    pthread_mutex_lock(&urb->lock);
}

void __urb_unlock(urb_t* urb) {
    pthread_mutex_unlock(&urb->lock);
}

void urb_init(urb_t* urb) {
	memset(urb, 0, sizeof(urb_t));
	assert(pthread_mutex_init(&urb->lock, NULL) == 0);
	assert(pthread_cond_init(&urb->done_cond, NULL) == 0);
}

urb_t* urb_create(usbip_op_cmd_submit_t* submit, uint8_t* data) {
	urb_t* urb = (urb_t*)malloc(sizeof(urb_t));
	urb_init(urb);
	if (submit != NULL) {
		memcpy(&urb->submit, submit, sizeof(urb->submit));
	}
	if (data != NULL) {	
		urb->data = data;
	}	
	return urb;
}

void urb_destroy(urb_t* urb) {
	if (urb == NULL) {
		return;
	}
	if (urb->sys != NULL) {
		sys_urb_unlink(urb);
	}
	assert(pthread_mutex_destroy(&urb->lock) == 0);
	assert(pthread_cond_destroy(&urb->done_cond) == 0);
}

void urb_free(urb_t* urb) {
	urb_destroy(urb);
	free(urb);
}	

void urb_wait(urb_t* urb, int timeout_ms) {
	__urb_lock(urb);
	while (!urb->done) {
		int r;
		struct timespec end_time = {0, 0};
		clock_gettime(CLOCK_REALTIME, &end_time);
		end_time.tv_nsec += timeout_ms * 1000;
		if (end_time.tv_nsec >= 1000000000L) {
			end_time.tv_nsec -= 1000000000L;
			end_time.tv_sec  += 1;
		}
		r = pthread_cond_timedwait(&urb->done_cond, &urb->lock, &end_time);
		if (r < 0) {
			break;
		}
	}
	__urb_unlock(urb);
}

void urb_set_done_handler(urb_t* urb, urb_handler_t done_handler, void* done_args) {
	__urb_lock(urb);
	urb->done_handler = done_handler;
	urb->done_arg = done_args;
	__urb_unlock(urb);
}

void urb_clear(urb_t* urb) {
	__urb_lock(urb);
	urb->done = 0;	
	__urb_unlock(urb);
}

void urb_done(urb_t* urb) {
	void* args;
	DBG_MSG("URB done (ep:%d)\n", urb->submit.ep);
	urb_handler_t handler;
	if (urb == NULL) {
		return;
	}
	__urb_lock(urb);
	urb->done = 1;
	handler = urb->done_handler;
	args = urb->done_arg;
	__urb_unlock(urb);
	pthread_cond_signal(&urb->done_cond);		
	if (handler != NULL) {
		handler(urb, args);
	}
}

