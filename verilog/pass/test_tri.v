// Test: tri (tristate) net type
// Feature: Multi-driver nets with resolution
// Expected: Should fail - tri nets not yet implemented

module test_tri;
  tri bus;
  reg drive_a, drive_b;
  reg value_a, value_b;

  assign bus = drive_a ? value_a : 1'bz;
  assign bus = drive_b ? value_b : 1'bz;

  initial begin
    drive_a = 1;
    drive_b = 0;
    value_a = 1;
    value_b = 0;
  end
endmodule
