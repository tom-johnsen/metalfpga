// Test: fork/join parallel blocks
// Feature: Parallel execution blocks
// Expected: Should fail - fork/join not yet implemented

module test_fork_join;
  reg [7:0] a, b, c;

  initial begin
    fork
      a = 8'd10;
      b = 8'd20;
      c = 8'd30;
    join
  end
endmodule
