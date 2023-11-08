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
#ifndef __QUEUE_H
#define __QUEUE_H

#define QUEUE(name, sz) \
	volatile struct name { \
		int size; \
		int blok_size; \
		int length; \
		int start; \
		int stop; \
		unsigned char data[sz]; \
	} name = { sz, 0, 0, 0, 0, { 0 } };

extern void queue_init(volatile void* queue, int blok_size, unsigned char init_val);
extern void queue_put(volatile void* queue,  unsigned char value);
extern unsigned char queue_get(volatile void* queue);
extern int queue_is_empty(volatile void* queue);
extern int queue_is_full(volatile void* queue);
extern unsigned char* get_queue_data(volatile void* queue);
extern unsigned char* get_queue_blok(volatile void* queue);
extern int get_start_index(volatile void* queue);
extern int get_length(volatile void* queue);

extern void queue_init_atomic(int ipl, volatile void* queue, int blok_size, unsigned char init_val);
extern void queue_put_atomic(int ipl, volatile void* queue,  unsigned char value);
extern unsigned char queue_get_atomic(int ipl, volatile void* queue);
extern int queue_is_empty_atomic(int ipl, volatile void* queue);
extern int queue_is_full_atomic(int ipl, volatile void* queue);
extern unsigned char* get_queue_data_atomic(int ipl, volatile void* queue);
extern unsigned char* get_queue_blok_atomic(int ipl, volatile void* queue);
extern int get_start_index_atomic(int ipl, volatile void* queue);
extern int get_length_atomic(int ipl, volatile void* queue);

#endif
