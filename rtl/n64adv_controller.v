//////////////////////////////////////////////////////////////////////////////////
//
// This file is part of the N64 RGB/YPbPr DAC project.
//
// Copyright (C) 2015-2021 by Peter Bartmann <borti4938@gmail.com>
//
// N64 RGB/YPbPr DAC is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////////
//
// Company:  Circuit-Board.de
// Engineer: borti4938
//
// Module Name:    n64adv_controller
// Project Name:   N64 Advanced RGB/YPbPr DAC Mod
// Target Devices: universial (PLL and 50MHz clock required)
// Tool versions:  Altera Quartus Prime
// Description:
// Latest change: ip independet implementation of RAM
//
//////////////////////////////////////////////////////////////////////////////////


module n64adv_controller (
  CLKs,
  nRST,
  nSRST,

  CTRL,

  PPUState,
  JumperCfgSet,
  USE_VPLL,
  PPUConfigSet,
  OSD_VSync,
  OSDWrVector,
  OSDInfo,

  VCLK,
  nVDSYNC,
  VD_VSi,
  nVRST
);

parameter [11:0] hdl_fw = 12'h000; // number is a dummy; defined in and passed from top module

`include "../vh/n64adv_cparams.vh"
`include "../vh/n64adv_ppuconfig.vh"

input [2:0] CLKs;
inout nRST;
input [2:0] nSRST;

input CTRL;

input      [12:0] PPUState;
input      [ 7:0] JumperCfgSet;
output reg        USE_VPLL;
output reg [68:0] PPUConfigSet;
input             OSD_VSync;
output     [24:0] OSDWrVector;
output reg [ 1:0] OSDInfo;

input VCLK;
input nVDSYNC;
input VD_VSi;
input nVRST;


// start of rtl

// misc stuff
wire CLK_4M = CLKs[2];
wire CLK_16k = CLKs[1];
wire CLK_50M = CLKs[0];

wire nSRST_4M = nSRST[2];
wire nSRST_50M = nSRST[0];


// parameters
localparam ST_WAIT4N64 = 2'b00; // wait for N64 sending request to controller
localparam ST_N64_RD   = 2'b01; // N64 request sniffing
localparam ST_CTRL_RD  = 2'b10; // controller response


// wires
wire [ 9:0] vd_wraddr;
wire [ 1:0] vd_wrctrl;
wire [12:0] vd_wrdata;

wire FallbackMode_resynced, FallbackMode_valid_resynced;
wire [12:0] PPUState_resynced;
wire OSD_VSync_resynced;

wire nVSYNC_buf_resynced;
wire new_ctrl_data_resynced;
wire ctrl_data_tack, ctrl_data_tack_resynced;

wire [31:0] SysConfigSet2, SysConfigSet1 ,SysConfigSet0;  // general structure of ConfigSet -> see vh/n64adv_ppuconfig.vh

wire use_igr_resynced;

wire ctrl_negedge, ctrl_posedge;
wire ctrl_bit;


// registers
reg nVSYNC_buf = 1'b0;

reg [9:0] time_out = 10'd1023;
reg FallbackMode  = 1'b0;
reg FallbackMode_valid = 1'b0;

reg [1:0] ctrl_data_tack_resynced_L = 2'b00;
reg [1:0] nVSYNC_resynced_L = 2'b00;
reg use_igr;

reg [1:0] rd_state = 2'b0;  // state machine for controller sniffing

reg [7:0] wait_cnt = 8'h0;  // counter for wait state (needs appr. 64us at CLK_4M clock to fill up from 0 to 255 -> hopefully enough for slow reacting controller like the wireless brawler)
reg [2:0] ctrl_hist = 3'h7;

reg [7:0] ctrl_low_cnt = 8'h0;
reg [31:0] serial_data[0:2];
initial begin
  serial_data[2] <= 32'h0;
  serial_data[1] <= 32'h0;
  serial_data[0] <= 32'h0;
end
reg [ 4:0] ctrl_data_cnt = 5'h0;
reg [ 1:0] new_ctrl_data = 2'b00;

reg initiate_nrst = 1'b0;
reg drv_rst = 1'b0;
reg [9:0] rst_cnt = 10'b0; // ~64ms are needed to count from max downto 0 with CLK_16k.


// logic

always @(posedge VCLK or negedge nVRST)
  if (!nVRST) begin
    nVSYNC_buf <= 1'b0;
  end else begin
    if (!nVDSYNC) begin
      nVSYNC_buf <= VD_VSi;
    end
  end

// Part 1: Instantiate NIOS II
// ===========================

always @(posedge VCLK)
  if (!FallbackMode_valid) begin
    if (~|time_out) begin
      FallbackMode <= ~nVRST;
      FallbackMode_valid <= 1'b1;
    end
    time_out <= time_out - 10'd1;
  end


register_sync #(
  .reg_width(17), // 13 + 1 + 1 + 1 + 1
  .reg_preset(17'd0)
) sync4cpu_u(
  .clk(CLK_50M),
  .clk_en(1'b1),
  .nrst(1'b1),
  .reg_i({PPUState,FallbackMode,FallbackMode_valid,new_ctrl_data[1],OSD_VSync}),
  .reg_o({PPUState_resynced,FallbackMode_resynced,FallbackMode_valid_resynced,new_ctrl_data_resynced,OSD_VSync_resynced})
);



system_n64adv1 system_u(
  .clk_clk(CLK_50M),
  .rst_reset_n(nSRST_50M),
  .sync_in_export({new_ctrl_data_resynced,OSD_VSync_resynced}),
  .vd_wraddr_export(vd_wraddr),
  .vd_wrctrl_export(vd_wrctrl),
  .vd_wrdata_export(vd_wrdata),
  .ctrl_data_in_export(serial_data[2]),
  .ctrl_data_tack_out_export(ctrl_data_tack),
  .jumper_cfg_set_in_export(JumperCfgSet),
  .ppu_state_in_export(PPUState_resynced),
  .fallback_in_export({FallbackMode_resynced,FallbackMode_valid_resynced}),
  .cfg_set2_out_export(SysConfigSet2),
  .cfg_set1_out_export(SysConfigSet1),
  .cfg_set0_out_export(SysConfigSet0),
  .hdl_fw_in_export(hdl_fw)
);

register_sync #(
  .reg_width(1),
  .reg_preset(1'b0)
) vsync2sysclk_u(
  .clk(CLK_50M),
  .clk_en(1'b1),
  .nrst(1'b1),
  .reg_i(nVSYNC_buf),
  .reg_o(nVSYNC_buf_resynced)
);

always @(*) begin
  USE_VPLL <= SysConfigSet2[`use_vpll_bit];
  use_igr <= SysConfigSet2[`igr_reset_enable_bit];
end

always @(posedge CLK_50M) begin
  if (nVSYNC_resynced_L[1] & !nVSYNC_resynced_L[0]) begin
    OSDInfo[1]       <= &{SysConfigSet2[`show_osd_logo_bit],SysConfigSet2[`show_osd_bit],!SysConfigSet2[`mute_osd_bit]};  // show logo only in OSD
    OSDInfo[0]       <= SysConfigSet2[`show_osd_bit] & !SysConfigSet2[`mute_osd_bit];
    PPUConfigSet     <= {SysConfigSet2[`show_testpattern_direct_bit],SysConfigSet2[`Exchange_RB_out_direct_bit],
                         SysConfigSet2[`FilterSet_slice_direct],SysConfigSet1,SysConfigSet0};
  end
  nVSYNC_resynced_L <= {nVSYNC_resynced_L[0],nVSYNC_buf_resynced};
end


assign OSDWrVector = {vd_wrctrl,vd_wraddr,vd_wrdata};


// Part 2: Controller Sniffing
// ===========================

register_sync #(
  .reg_width(2),
  .reg_preset(2'b0)
) useigr2clk4m_u(
  .clk(CLK_4M),
  .clk_en(1'b1),
  .nrst(1'b1),
  .reg_i({use_igr,ctrl_data_tack}),
  .reg_o({use_igr_resynced,ctrl_data_tack_resynced})
);

assign ctrl_negedge  =  ctrl_hist[2] & !ctrl_hist[1];
assign ctrl_posedge  = !ctrl_hist[2] &  ctrl_hist[1];

assign ctrl_bit = ctrl_low_cnt < wait_cnt;

// controller data bits:
//  0: 7 - A, B, Z, St, Du, Dd, Dl, Dr
//  8:15 - 'Joystick reset', (0), L, R, Cu, Cd, Cl, Cr
// 16:23 - X axis
// 24:31 - Y axis
// 32    - Stop bit

always @(posedge CLK_4M or negedge nSRST_4M)
  if (!nSRST_4M) begin
    rd_state       <= ST_WAIT4N64;
    wait_cnt        <=  8'h0;
    ctrl_hist       <=  3'h7;
    ctrl_low_cnt    <=  8'h0;
    serial_data[2]  <= 32'h0;
    serial_data[1]  <= 32'h0;
    serial_data[0]  <= 32'h0;
    ctrl_data_cnt   <=  5'h0;
    new_ctrl_data   <=  2'b0;
    initiate_nrst   <=  1'b0;
    ctrl_data_tack_resynced_L <= 2'b00;
  end else begin
    case (rd_state)
      ST_WAIT4N64:
        if (&wait_cnt & ctrl_negedge) begin // waiting duration ends (exit wait state only if CTRL was high for a certain duration)
          rd_state       <= ST_N64_RD;
          serial_data[0] <= 32'h0;
          ctrl_data_cnt  <=  5'h0;
        end
      ST_N64_RD: begin
        if (ctrl_posedge)       // sample data part 1
          ctrl_low_cnt <= wait_cnt;
        if (ctrl_negedge) begin // sample data part 2
          if (!ctrl_data_cnt[3]) begin  // eight bits read
            serial_data[0][29:22] <= {ctrl_bit,serial_data[0][29:23]};
            ctrl_data_cnt         <=  ctrl_data_cnt + 1'b1;
          end else if (serial_data[0][29:22] == 8'b10000000) begin // check command
            rd_state       <= ST_CTRL_RD;
            serial_data[0] <= 32'h0;
            ctrl_data_cnt  <=  5'h0;
          end else begin
            rd_state <= ST_WAIT4N64;
          end
        end
      end
      ST_CTRL_RD: begin
        if (ctrl_posedge)       // sample data part 1
          ctrl_low_cnt <= wait_cnt;
        if (ctrl_negedge) begin // sample data part 2
          if (~&ctrl_data_cnt) begin  // still reading
            serial_data[0] <= {ctrl_bit,serial_data[0][31:1]};
            ctrl_data_cnt  <=  ctrl_data_cnt + 1'b1;
          end else begin  // thirtytwo bits read
            rd_state         <= ST_WAIT4N64;
            serial_data[1]   <= {ctrl_bit,serial_data[0][31:1]};
            new_ctrl_data[0] <= 1'b1;  // signalling new controller data available
          end
        end
      end
      default: begin
        rd_state <= ST_WAIT4N64;
      end
    endcase

    if (ctrl_negedge | ctrl_posedge) begin // counter reset
      wait_cnt <= 8'h0;
    end else begin
      if (~&wait_cnt) // saturate counter if needed
        wait_cnt <= wait_cnt + 1'b1;
      else  // counter saturated
        rd_state <= ST_WAIT4N64;
    end

    ctrl_hist <= {ctrl_hist[1:0],CTRL};

    if (new_ctrl_data[0] && new_ctrl_data[0]) begin
      serial_data[2] <= serial_data[1];
      new_ctrl_data  <= 2'b10;
    end
    if (^ctrl_data_tack_resynced_L)
      new_ctrl_data[1] <= 1'b0;
    ctrl_data_tack_resynced_L <= {ctrl_data_tack_resynced_L[0],ctrl_data_tack_resynced};
    
    if (use_igr_resynced & (serial_data[1][15:0] == `IGR_RESET))
      initiate_nrst <= 1'b1;
  end


// Part 3: Trigger Reset on Demand
// ===============================

always @(posedge CLK_16k)
  if (initiate_nrst == 1'b1) begin
    drv_rst <= 1'b1;            // reset system
    rst_cnt <= 10'h3ff;
  end else if (|rst_cnt) begin  // decrement as long as rst_cnt is not zero
    rst_cnt <= rst_cnt - 1'b1;
  end else begin
    drv_rst <= 1'b0;            // end of reset
  end

assign nRST = drv_rst ? 1'b0 : 1'bz;

endmodule
