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
 * n64.h
 *
 *  Created on: 06.01.2018
 *      Author: Peter Bartmann
 *
 ********************************************************************************/


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "alt_types.h"
#include "altera_avalon_pio_regs.h"
#include "system.h"
#include "config.h"
#include "n64.h"
#include "vd_driver.h"

#define ANALOG_TH     50

#define HOLD_CNT_LOW	5
#define HOLD_CNT_MID0	8
#define HOLD_CNT_MID1	12
#define HOLD_CNT_HIGH	17

#define HOLD_CNT_REP	2

#define VPLL_LOST_TH      10
#define VPLL_INSTABLE_TH  13
#define VPLL_STABLE_TH    15

static const char *running_message = "< Running >";

alt_u32 ctrl_data;
vpll_lock_state_t vpll_lock_state;
alt_u16 ppu_state;
vmode_t palmode;
scanmode_t scanmode;
linemult_t linemult_mode;



void update_ppu_state()
{
  ppu_state = (IORD_ALTERA_AVALON_PIO_DATA(PPU_STATE_IN_BASE) & PPU_STATE_GETALL_MASK);
  palmode = ((ppu_state & PPU_STATE_PALMODE_GETMASK) >> PPU_STATE_PALMODE_OFFSET);
  scanmode = ((ppu_state & PPU_STATE_480I_GETMASK) >> PPU_STATE_480I_OFFSET);
  linemult_mode = ((ppu_state & PPU_STATE_LINEMULT_GETMASK) >> PPU_STATE_LINEMULT_OFFSET);
}

void update_ctrl_data()
{
  static cfg_offon_t tack = 0;
  ctrl_data = IORD_ALTERA_AVALON_PIO_DATA(CTRL_DATA_IN_BASE);
  tack = ~tack;
  IOWR_ALTERA_AVALON_PIO_DATA(CTRL_DATA_TACK_OUT_BASE,tack);
}

void update_vpll_lock_state()
{
  static alt_u8 vpll_lock_cnt = VPLL_INSTABLE_TH;
  static cfg_offon_t vpll_lock;
  vpll_lock = ((IORD_ALTERA_AVALON_PIO_DATA(PPU_STATE_IN_BASE) & PPU_STATE_VPLL_LOCKED_GETMASK) >> PPU_STATE_VPLL_LOCKED_OFFSET);
  if (vpll_lock) {
    if (vpll_lock_cnt < VPLL_STABLE_TH) vpll_lock_cnt++;
    else                                vpll_lock_cnt = VPLL_STABLE_TH;
  } else {
    if (vpll_lock_cnt > 0) vpll_lock_cnt--;
    else                   vpll_lock_cnt = 0;
  }
  vpll_lock_state = vpll_lock_cnt == VPLL_STABLE_TH   ? VPLL_LOCK_STABLE :
                    vpll_lock_cnt >= VPLL_INSTABLE_TH ? VPLL_LOCK_INSTABLE :
                                                        VPLL_LOCK_LOST;
}

cmd_t ctrl_data_to_cmd(cfg_offon_t no_fast_skip)
{
  cmd_t cmd_new = CMD_NON;
  static cmd_t cmd_pre_int = CMD_NON, cmd_pre = CMD_NON;

  static alt_u8 rep_cnt = 0, hold_cnt = HOLD_CNT_HIGH;

  switch (ctrl_data & CTRL_GETALL_DIGITAL_MASK) {
    case BTN_OPEN_OSDMENU:
      cmd_new = CMD_OPEN_MENU;
      break;
    case BTN_CLOSE_OSDMENU:
      cmd_new = CMD_CLOSE_MENU;
      break;
    case BTN_MUTE_OSDMENU:
      cmd_new = CMD_MUTE_MENU;
      break;
    case BTN_DEBLUR_QUICK_ON:
      cmd_new = CMD_DEBLUR_QUICK_ON;
      break;
    case BTN_DEBLUR_QUICK_OFF:
      cmd_new = CMD_DEBLUR_QUICK_OFF;
      break;
    case BTN_16BIT_QUICK_ON:
      cmd_new = CMD_16BIT_QUICK_ON;
      break;
    case BTN_16BIT_QUICK_OFF:
      cmd_new = CMD_16BIT_QUICK_OFF;
      break;
    case BTN_MENU_ENTER:
      cmd_new = CMD_MENU_ENTER;
      break;
    case BTN_MENU_BACK:
      cmd_new = CMD_MENU_BACK;
      break;
    case CTRL_R_SETMASK:
      cmd_new = CMD_MENU_PAGE_RIGHT;
      break;
    case CTRL_L_SETMASK:
    case CTRL_Z_SETMASK:
      cmd_new = CMD_MENU_PAGE_LEFT;
      break;
    case CTRL_DU_SETMASK:
    case CTRL_CU_SETMASK:
      cmd_new = CMD_MENU_UP;
      break;
    case CTRL_DD_SETMASK:
    case CTRL_CD_SETMASK:
      cmd_new = CMD_MENU_DOWN;
      break;
    case CTRL_DL_SETMASK:
    case CTRL_CL_SETMASK:
      cmd_new = CMD_MENU_LEFT;
      break;
    case CTRL_DR_SETMASK:
    case CTRL_CR_SETMASK:
      cmd_new = CMD_MENU_RIGHT;
      break;
  };

  if (cmd_new == CMD_NON) {
		alt_u16 xy_axis = ALT_CI_NIOS_CUSTOM_INSTR_BITSWAP_0(ctrl_data);
		alt_8 x_axis_val = xy_axis >> 8;
		alt_8 y_axis_val = xy_axis;

		if (x_axis_val  >  ANALOG_TH) cmd_new = CMD_MENU_RIGHT;
		if (x_axis_val  < -ANALOG_TH) cmd_new = CMD_MENU_LEFT;
		if (y_axis_val  >  ANALOG_TH) cmd_new = CMD_MENU_UP;
		if (y_axis_val  < -ANALOG_TH) cmd_new = CMD_MENU_DOWN;
  }

  if (cmd_pre_int != cmd_new) {
    cmd_pre_int = cmd_new;
    rep_cnt = 0;
    hold_cnt = HOLD_CNT_HIGH;
  } else {
    if (hold_cnt == 0) {
      if (rep_cnt < 255) rep_cnt++;
      hold_cnt = HOLD_CNT_LOW;
      if (rep_cnt < 3*HOLD_CNT_REP) hold_cnt = HOLD_CNT_MID1;
      if (rep_cnt < 2*HOLD_CNT_REP) hold_cnt = HOLD_CNT_MID0;
      if (rep_cnt <   HOLD_CNT_REP) hold_cnt = HOLD_CNT_HIGH;
      if (!no_fast_skip) cmd_pre = CMD_NON;
    } else {
      hold_cnt--;
    }
  }

  if (cmd_pre != cmd_new) {
    if (cmd_pre == CMD_MUTE_MENU && cmd_new == CMD_NON)
      cmd_new = CMD_UNMUTE_MENU;
    cmd_pre = cmd_new;
    return cmd_new;
  }

  return CMD_NON;
}

cfg_offon_t get_osdvsync()
{
  return (IORD_ALTERA_AVALON_PIO_DATA(SYNC_IN_BASE) & OSD_VSYNC_IN_MASK);
}

cfg_offon_t new_ctrl_available()
{
  return (IORD_ALTERA_AVALON_PIO_DATA(SYNC_IN_BASE) & NEW_CTRL_DATA_IN_MASK);
}

cfg_offon_t get_fallback_mode()
{
  return ((IORD_ALTERA_AVALON_PIO_DATA(FALLBACK_IN_BASE) & FALLBACK_GETALL_MASK) >> FALLBACKMODE_OFFSET);
}

cfg_offon_t is_fallback_mode_valid()
{
  return (IORD_ALTERA_AVALON_PIO_DATA(FALLBACK_IN_BASE) & FALLBACKMODE_VALID_GETMASK);
}

alt_u16 get_hdl_version()
{
  return (IORD_ALTERA_AVALON_PIO_DATA(HDL_FW_IN_BASE) & HDL_FW_GETALL_MASK);
}
