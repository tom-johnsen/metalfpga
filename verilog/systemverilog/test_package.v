// Test: SystemVerilog package
// Feature: Package declarations and imports
// Expected: Should fail - SystemVerilog construct

package my_pkg;
  parameter WIDTH = 8;
  typedef reg [WIDTH-1:0] data_t;

  function [WIDTH-1:0] double_value;
    input [WIDTH-1:0] x;
    double_value = x << 1;
  endfunction
endpackage

module test_package;
  import my_pkg::*;
  data_t value;

  initial begin
    value = double_value(8'h10);
  end
endmodule
