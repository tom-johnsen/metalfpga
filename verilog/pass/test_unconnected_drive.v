// EXPECT=PASS
// Test: `unconnected_drive compiler directive
// Feature: Specify value for unconnected ports

`unconnected_drive pull1

module sub (input a, b, output y);
  assign y = a & b;
endmodule

module test_unconnected_drive;
  wire out;
  reg in1;

  sub u1 (.a(in1), .y(out));  // b is unconnected, should be pulled to 1
endmodule

`nounconnected_drive
