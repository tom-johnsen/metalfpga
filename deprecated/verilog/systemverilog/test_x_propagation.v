// Test: X (unknown) value propagation
// Feature: How X values propagate through operators
// Expected: Should pass - testing 4-state logic

module test_x_propagation;
  reg a, b, c;
  wire [3:0] results;

  assign results[0] = 1'bx & 1'b1;  // X & 1 = X
  assign results[1] = 1'bx & 1'b0;  // X & 0 = 0 (optimistic)
  assign results[2] = 1'bx | 1'b1;  // X | 1 = 1 (optimistic)
  assign results[3] = 1'bx | 1'b0;  // X | 0 = X

  initial begin
    a = 1'bx;
    b = 1'b1;
    c = a & b;  // Should be X
  end
endmodule
