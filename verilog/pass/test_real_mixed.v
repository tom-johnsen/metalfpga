// Test: Mixed real and integer arithmetic
// Feature: Type promotion in expressions
// Expected: Should fail - real type not yet implemented

module test_real_mixed;
  real r;
  integer i;
  reg [7:0] byte_val;

  initial begin
    i = 42;
    byte_val = 8'h10;

    r = i + 3.5;           // Integer promotes to real
    r = byte_val * 2.0;    // Reg promotes to real
    i = r;                 // Real truncates to integer
  end
endmodule
