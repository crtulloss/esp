`timescale 1 ps / 1 ps
// Copyright (c) 2014-2015, Columbia University
module GSRAM_1024x16( CLK, A0, D0, Q0, WE0, WEM0, CE0, A1, D1, Q1, WE1, WEM1, CE1 );

    input CLK;
    input [9:0] A0;
    input [15:0] D0;
    output [15:0] Q0;
    input WE0;
    input [15:0] WEM0;
    input CE0;
    input [9:0] A1;
    input [15:0] D1;
    output [15:0] Q1;
    input WE1;
    input [15:0] WEM1;
    input CE1;

    reg [9:0]  ADDR0;
    reg [9:0]  ADDR1;
    reg WEN0;
    reg WEN1;
    reg [15:0] DATA0;
    reg [15:0] DATA1;

    always @(*) begin
	#5
	ADDR0 = A0;
	ADDR1 = A1;
	WEN0 = WE0 & CE0;
	WEN1 = WE1 & CE1;
	DATA0 = D0;
	DATA1 = D1;
    end
  
    // generic SRAM for comp-kernel-only synthesis
    generic_sram
        #(
          .abits(10),
          .dbits(16)
         )
    sram
        (
         .clk(CLK),
         .a0(ADDR0),
         .a1(ADDR1),
         .d0(DATA0),
         .d1(DATA1),
         .q0(Q0),
         .q1(Q1),
         .we0(WEN0),
         .we1(WEN1)
        );

endmodule
