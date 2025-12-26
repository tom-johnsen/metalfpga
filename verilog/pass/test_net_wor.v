// Test: wor net type (wired-OR)
// Feature: Net types with implicit logic
// Expected: Should fail - wor not yet implemented

module test_net_wor(
  input a,
  input b,
  output wor y  // Wired-OR: y = a | b when both drive
);
  assign y = a;
  assign y = b;  // Multiple drivers on wor net
endmodule
