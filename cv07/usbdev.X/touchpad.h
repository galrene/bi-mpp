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
#ifndef __TOUCHPAD_H
#define __TOUCHPAD_H

extern void touchpad_init();
extern int get_touchpad_key();
extern int get_touchpad_status();
extern int touchpad_read(int index);
extern int get_touchpad_threshold();
extern void set_touchpad_threshold(int thr);

#endif

