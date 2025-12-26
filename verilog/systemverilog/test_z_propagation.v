// Test: Z (high-impedance) value propagation
// Feature: How Z values propagate and resolve
// Expected: Should pass - testing tristate logic

module test_z_propagation;
  reg a, b;
  wire w;

  assign w = a;
  assign w = b;

  initial begin
    a = 1'bz; b = 1'b1;  // Z + 1 = 1
    #10 a = 1'b0; b = 1'bz;  // 0 + Z = 0
    #10 a = 1'bz; b = 1'bz;  // Z + Z = Z
    #10 a = 1'b0; b = 1'b1;  // 0 + 1 = X (conflict)
  end
endmodule
