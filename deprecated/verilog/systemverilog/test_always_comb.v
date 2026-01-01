// Test: SystemVerilog always_comb
// Feature: always_comb procedural block
// Expected: Should fail - SystemVerilog construct

module test_always_comb;
  reg [7:0] a, b;
  reg [7:0] sum;

  always_comb begin
    sum = a + b;
  end

  initial begin
    a = 8'h10;
    b = 8'h20;
  end
endmodule
