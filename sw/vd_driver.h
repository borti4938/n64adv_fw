/*********************************************************************************
 *
 * This file is part of the N64 RGB/YPbPr DAC project.
 *
 * Copyright (C) 2015-2022 by Peter Bartmann <borti4938@gmail.com>
 *
 * N64 RGB/YPbPr DAC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *********************************************************************************
 *
 * vd_driver.h
 *
 *  Created on: 06.01.2018
 *      Author: Peter Bartmann
 *
 ********************************************************************************/

#include "alt_types.h"
#include "altera_avalon_pio_regs.h"

#ifndef VD_DRIVER_H_
#define VD_DRIVER_H_

// define virtual display (memory mapped)
#define VD_WIDTH  52
#define VD_HEIGHT 12

// define some masks and shifts
#define VD_WRDATA_BACKGR_OFFSET 11
#define VD_WRDATA_COLOR_OFFSET   7
#define VD_WRDATA_TEXT_OFFSET    0

#define VD_WRDATA_ALL_ANDMASK   0x1FFF
#define VD_WRDATA_BG_ANDMASK    0x1800
#define VD_WRDATA_COLOR_ANDMASK 0x0780
#define VD_WRDATA_FONT_ANDMASK  0x007F


#define VD_WRADDR_HSHIFT_OFFSET 4

#define VD_WRADDR_V_ANDMASK 0x00F
#define VD_WRADDR_H_ANDMASK 0x3F0


#define VD_WRCTRL_GETALL_MASK         0x3
#define VD_WRCTRL_WREN_GETMASK        0x3
#define VD_WRCTRL_WREN_SETMASK        0x3
#define VD_WRCTRL_WREN_CLRMASK        (VD_WRCTRL_GETALL_MASK & ~VD_WRCTRL_WREN_SETMASK)
  #define VD_WRCTRL_WREN_COLOR_GETMASK  0x2
  #define VD_WRCTRL_WREN_COLOR_SETMASK  0x2
  #define VD_WRCTRL_WREN_COLOR_CLRMASK  (VD_WRCTRL_GETALL_MASK & ~VD_WRCTRL_WREN_COLOR_SETMASK)
  #define VD_WRCTRL_WREN_FONT_GETMASK   0x1
  #define VD_WRCTRL_WREN_FONT_SETMASK   0x1
  #define VD_WRCTRL_WREN_FONT_CLRMASK   (VD_WRCTRL_GETALL_MASK & ~VD_WRCTRL_WREN_FONT_SETMASK)
#define VD_WRCTRL_WRTXTEN_GETMASK     0x1
#define VD_WRCTRL_WRTXTEN_SETMASK     0x1
#define VD_WRCTRL_WRTXTEN_CLRMASK     (VD_WRCTRL_GETALL_MASK & ~VD_WRCTRL_WRTXTEN_SETMASK)
#define VD_WRCTRL_WRCOLEN_GETMASK     0x2
#define VD_WRCTRL_WRCOLEN_SETMASK     0x2
#define VD_WRCTRL_WRCOLEN_CLRMASK     (VD_WRCTRL_GETALL_MASK & ~VD_WRCTRL_WRCOLEN_SETMASK)


// Color definitions
#define BACKGROUNDCOLOR_WHITE     0x3
#define BACKGROUNDCOLOR_GREY      0x2
#define BACKGROUNDCOLOR_BLACK     0x1
#define BACKGROUNDCOLOR_STANDARD  0x0

#define FONTCOLOR_NON         0x0
#define FONTCOLOR_BLACK       0x1
#define FONTCOLOR_GREY        0x2
#define FONTCOLOR_LIGHTGREY   0x3
#define FONTCOLOR_WHITE       0x4
#define FONTCOLOR_RED         0x5
#define FONTCOLOR_GREEN       0x6
#define FONTCOLOR_BLUE        0x7
#define FONTCOLOR_YELLOW      0x8
#define FONTCOLOR_CYAN        0x9
#define FONTCOLOR_MAGENTA     0xA
#define FONTCOLOR_DARKORANGE  0xB
#define FONTCOLOR_TOMATO      0xC
#define FONTCOLOR_DARKMAGENTA 0xD
#define FONTCOLOR_NAVAJOWHITE 0xE
#define FONTCOLOR_DARKGOLD    0xF

// some special chars
#define EMPTY               0x00
#define TRIANGLE_LEFT       0x10
#define TRIANGLE_RIGHT      0x11
#define ARROW_LEFT          0x1A
#define ARROW_RIGHT         0x1B
#define OPEN_TRIANGLE_LEFT  0x3C
#define OPEN_TRIANGLE_RIGHT 0x3E

// some macros
#define VD_SET_ADDR(h,v)   IOWR_ALTERA_AVALON_PIO_DATA(VD_WRADDR_BASE,((h<<VD_WRADDR_HSHIFT_OFFSET) & VD_WRADDR_H_ANDMASK) | \
                                                                       (v & VD_WRADDR_V_ANDMASK))
#define VD_SET_DATA(b,c,f) IOWR_ALTERA_AVALON_PIO_DATA(VD_WRDATA_BASE,(((b<<VD_WRDATA_BACKGR_OFFSET) & VD_WRDATA_BG_ANDMASK)    | \
                                                                       ((c<<VD_WRDATA_COLOR_OFFSET)  & VD_WRDATA_COLOR_ANDMASK) | \
                                                                        (f & VD_WRDATA_FONT_ANDMASK)                            ))
#define VD_CLEAR_SCREEN  vd_clear_area(0,VD_WIDTH,0,VD_HEIGHT)

// prototypes
int vd_clear_area(alt_u8 horiz_offset_start, alt_u8 horiz_offset_stop, alt_u8 vert_offset_start, alt_u8 vert_offset_stop);
static int inline vd_clear_lineend (alt_u8 horiz_offset_start, alt_u8 vert_offset)
  { return vd_clear_area(horiz_offset_start, VD_WIDTH-1, vert_offset, vert_offset); }
static int inline vd_clear_fullline (alt_u8 vert_offset)
  { return vd_clear_area(0, VD_WIDTH-1, vert_offset, vert_offset); }
static int inline vd_clear_columnend (alt_u8 horiz_offset, alt_u8 vert_offset_start)
  { return vd_clear_area(horiz_offset, horiz_offset, vert_offset_start, VD_HEIGHT-1); }
static int inline vd_clear_fullcolumn (alt_u8 horiz_offset)
  { return vd_clear_area(horiz_offset, horiz_offset, 0, VD_HEIGHT-1); }
static int inline vd_clear_char (alt_u8 horiz_offset, alt_u8 vert_offset)
  { return vd_clear_area(horiz_offset, horiz_offset, vert_offset, vert_offset); }

int vd_print_string(alt_u8 horiz_offset, alt_u8 vert_offset, alt_u8 background, alt_u8 color, const char *string);
int vd_print_char(alt_u8 horiz_offset, alt_u8 vert_offset, alt_u8 background, alt_u8 color, const char character);

/* ToDo: still a bug somewhere here; NIOS II stops working if functions used */
//int vd_change_color_area(alt_u8 horiz_offset_start, alt_u8 horiz_offset_stop, alt_u8 vert_offset_start, alt_u8 vert_offset_stop, alt_u8 background, alt_u8 fontcolor);
//static int inline vd_change_color_lineend (alt_u8 horiz_offset_start, alt_u8 vert_offset, alt_u8 background, alt_u8 fontcolor)
//  { return vd_change_color_area(horiz_offset_start, VD_WIDTH-1, vert_offset, vert_offset, background, fontcolor); }
//static int inline vd_change_color_fullline (alt_u8 vert_offset, alt_u8 background, alt_u8 fontcolor)
//  { return vd_change_color_area(0, VD_WIDTH-1, vert_offset, vert_offset, background, fontcolor); }
//static int inline vd_change_color_columnend (alt_u8 horiz_offset, alt_u8 vert_offset_start, alt_u8 background, alt_u8 fontcolor)
//  { return vd_change_color_area(horiz_offset, horiz_offset, vert_offset_start, VD_HEIGHT-1, background, fontcolor); }
//static int inline vd_change_color_fullcolumn (alt_u8 horiz_offset, alt_u8 background, alt_u8 fontcolor)
//  { return vd_change_color_area(horiz_offset, horiz_offset, 0, VD_HEIGHT-1, background, fontcolor); }
//static int inline vd_change_color (alt_u8 horiz_offset, alt_u8 vert_offset, alt_u8 background, alt_u8 fontcolor)
//  { return vd_change_color_area(horiz_offset, horiz_offset, vert_offset, vert_offset, background, fontcolor); }
//
//int vd_change_color_px (alt_u8 horiz_offset, alt_u8 vert_offset, alt_u8 background, alt_u8 color);

void vd_write_data(alt_u8 wr_color, alt_u8 wr_font);

#endif /* VD_DRIVER_H_ */
