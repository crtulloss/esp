`timescale 1 ps / 1 ps
// Copyright (c) 2014-2015, Columbia University
module GSRAM_2048x8( CLK, A0, D0, Q0, WE0, CE0, A1, D1, Q1, WE1, CE1 );

    input CLK;
    input [10:0] A0;
    input [7:0] D0;
    output [7:0] Q0;
    input WE0;
    input CE0;
    input [10:0] A1;
    input [7:0] D1;
    output [7:0] Q1;
    input WE1;
    input CE1;

    reg [10:0]  ADDR0;
    reg [10:0]  ADDR1;
    reg WEN0;
    reg WEN1;
    reg [7:0] DATA0;
    reg [7:0] DATA1;

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
          .abits(11),
          .dbits(8)
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
