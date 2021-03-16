`timescale 1 ps / 1 ps
// Copyright (c) 2014-2015, Columbia University
module GSRAM_16384x1( CLK, A0, D0, Q0, WE0, WEM0, CE0, A1, D1, Q1, WE1, WEM1, CE1 );

    input CLK;
    input [13:0] A0;
    input D0;
    output Q0;
    input WE0;
    input WEM0;
    input CE0;
    input [13:0] A1;
    input D1;
    output Q1;
    input WE1;
    input WEM1;
    input CE1;

    reg [13:0]  ADDR0;
    reg [13:0]  ADDR1;
    reg WEN0;
    reg WEN1;
    reg DATA0;
    reg DATA1;

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
          .abits(14),
          .dbits(1)
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
