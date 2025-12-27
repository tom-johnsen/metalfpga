// EXPECT=PASS
// Test: Constant function evaluation
// Feature: Functions used in parameter/constant context

module test_const_function;
  function [7:0] compute_mask;
    input [3:0] width;
    integer i;
    begin
      compute_mask = 0;
      for (i = 0; i < width; i = i + 1)
        compute_mask[i] = 1;
    end
  endfunction

  parameter WIDTH = 5;
  parameter MASK = compute_mask(WIDTH);  // Constant function call

  wire [7:0] masked = 8'hFF & MASK;
endmodule
