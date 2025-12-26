// Test: Force on wire (continuous assignment)
// Feature: Override wire values
// Expected: Should fail - force/release not yet implemented

module test_force_wire;
  wire w;
  reg a, b;

  assign w = a & b;

  initial begin
    a = 1; b = 1;  // w should be 1
    #10;

    force w = 0;   // Override to 0
    #10;

    release w;     // Back to a & b = 1
    #10;

    a = 0;         // w should be 0
  end
endmodule
