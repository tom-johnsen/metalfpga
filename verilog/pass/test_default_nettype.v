// Test: `default_nettype compiler directive
// Feature: Change default net type for implicit declarations
// Expected: Should fail - `default_nettype not yet implemented

`default_nettype wire  // Default behavior

module sub1;
  assign implicit1 = 1'b1;  // Implicitly wire
endmodule

`default_nettype none  // Disable implicit nets

module sub2;
  // assign implicit2 = 1'b0;  // Would be an error
  wire explicit;
  assign explicit = 1'b0;
endmodule

`default_nettype wire  // Restore default

module test_default_nettype;
  sub1 u1();
  sub2 u2();
endmodule
