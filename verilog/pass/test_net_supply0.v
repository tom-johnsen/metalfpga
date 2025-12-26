// Test: supply0 net type (constant 0)
// Feature: Power/ground nets
// Expected: Should fail - supply nets not yet implemented

module test_net_supply0(
  output supply0 gnd
);
  // supply0 is always driven to logic 0 with supply strength
endmodule
