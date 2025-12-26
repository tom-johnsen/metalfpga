// Test: timescale directive
// Feature: Compiler directives
// Expected: Should fail - timescale not yet implemented

`timescale 1ns / 1ps

module test_timescale;
  reg clk;

  initial begin
    clk = 0;
    #10 clk = 1;
    #10 clk = 0;
  end
endmodule
