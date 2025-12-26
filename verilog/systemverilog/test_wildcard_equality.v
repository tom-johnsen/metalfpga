// Test: SystemVerilog wildcard equality operators
// Feature: ==? and !=? operators
// Expected: Should fail - SystemVerilog construct

module test_wildcard_equality;
  reg [7:0] data;
  reg match;

  always @* begin
    // Wildcard match: x and z are treated as don't care
    match = (data ==? 8'b1010_xxxx);
  end

  initial begin
    data = 8'b1010_0000;  // Should match
    #10 data = 8'b1011_0000;  // Should not match
  end
endmodule
