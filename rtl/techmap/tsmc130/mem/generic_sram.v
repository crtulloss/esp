`timescale 1 ps / 1 ps
// Copyright (c) 2014-2015, Columbia University
module generic_sram
#(parameter abits = 10, parameter dbits = 16)
(
    input clk,
    input [abits-1:0] a0,
    input [abits-1:0] a1,
    input [dbits-1:0] d0,
    input [dbits-1:0] d1,
    output [dbits-1:0] q0,
    output [dbits-1:0] q1,
    input we0,
    input we1
);

    reg [dbits-1:0] q0;
    reg [dbits-1:0] q1;

    reg [dbits-1:0] mem [2**abits-1:0];

    always @(posedge clk) begin
        if (we0)
            mem[a0] <= d0;
        if (we1)
            mem[a1] <= d1;
        q0 <= mem[a0];
        q1 <= mem[a1];
    end


endmodule
