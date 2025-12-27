// EXPECT=PASS
// Test: Fork/join with timing differences
// Feature: Parallel execution blocks with delays
// Expected: May fail - complex fork/join timing

module test_parallel_block;
  reg [7:0] a, b, c;

  initial begin
    fork
      #10 a = 8'h11;
      #5 b = 8'h22;
      #15 c = 8'h33;
    join
    // All should complete, c should be last at time 15
    a = a + b + c;
  end
endmodule
