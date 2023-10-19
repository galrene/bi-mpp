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
#ifndef __LED_H
#define __LED_H

#define LED_R 1
#define LED_G 2
#define LED_B 3

void led_init();
void led_set(int led, int v);
void led_on(int led);
void led_off(int led);
void led_toggle(int led);
void led_all_off();


#endif

