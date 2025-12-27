// Test: `resetall compiler directive
// Feature: Reset all compiler directives to defaults
// Expected: May pass - directive might be ignored

`timescale 1ns/1ps
`default_nettype none

`resetall  // Reset everything

module test_resetall;
  // Timescale and default_nettype should be back to defaults
  assign implicit_wire = 1'b1;
endmodule
