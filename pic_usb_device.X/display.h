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
#ifndef __DISPLAY_H
#define __DISPLAY_H

// Display initialization

extern void disp_init(); 
extern void disp_clear();

// Terminal

#define TERM_WIDTH 21
#define TERM_HEIGHT 8

extern void disp_char(char c);
extern void disp_str(char* c);
extern void disp_line(char* c);
extern void disp_home();
extern void disp_at(int row, int col);
extern void disp_nl();
extern int disp_get_row();
extern int disp_get_col();

// Graphics

#define COLOR_BLACK  0
#define COLOR_WHITE  1

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

extern void put_pixel(int x, int y, int color);
extern int  get_pixel(int x, int y);
extern void draw_line(int x1, int y1, int x2, int y2);
extern void draw_rect(int x1, int y1, int x2, int y2);
extern void draw_ellipse(int x, int y, int w, int h);
extern void draw_char(int x, int y, char c);
extern void draw_string(int x, int y, char* s);
extern void draw_char_at_line(int row, int col, char c);
extern int  get_char_width(char c);
extern int  get_string_width(char* s);
extern void set_color(int color);

// low-level SH1101 functions

#define COMMAND      0
#define DATA         1

#define SET_LOWER_COLUMN_ADDRESS  0x00
#define SET_HIGHER_COLUMN_ADDRESS 0x10
#define SET_DISPLAY_START_LINE    0x40
#define SET_CONTRAST_CONTROL_REGISTER 0x81
#define SET_SEGMENT_REMAP_RIGHT 0xA0
#define SET_SEGMENT_REMAP_LEFT 0xA1
#define SET_ENTIRE_DISP_ON 0xA5
#define SET_ENTIRE_DISP_NORMAL 0xA4
#define SET_NORMAL_DISPLAY 0xA6
#define SET_REVERSE_DISPLAY 0xA7
#define SET_MULTIPLEX_RATIO 0xA8
#define SET_DC_DC 0xAD
#define DC_DC_ON 0x8B
#define DC_DC_OFF 0x8A
#define SET_DISPLAY_ON 0xAF
#define SET_DISPLAY_OFF 0xAE
#define SET_PAGE_ADDRESS 0xB0
#define SET_COMMON_OUTPUT_SCAN_DIR_NORM 0xC0
#define SET_COMMON_OUTPUT_SCAN_DIR_FLIPPED 0xC4
#define SET_DISPLAY_OFFSET  0xD3
#define SET_DISPLAY_CLOCK_DIVIDE_RATIO 0xD5
#define SET_DIS_PRE_CHARGE_PERIOD 0xD9
#define SET_COMMON_PADS_HW_CONFIG 0xDA
#define SET_VCOM_DESEL_LEVEL 0xDB
#define SET_READ_MODIFY_WRITE_MODE 0xE0
#define END 0xEE
#define NOP 0xE3

extern void sh1101_reset();
extern void sh1101_write(int addr, unsigned char v);
extern unsigned char sh1101_read(int addr);

#endif

