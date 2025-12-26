// Test: SystemVerilog streaming operators
// Feature: {<<} and {>>} streaming concatenation
// Expected: Should fail - SystemVerilog construct

module test_streaming_operator;
  reg [31:0] data;
  reg [31:0] reversed;

  initial begin
    data = 32'hDEADBEEF;
    // Bit-reverse using streaming operator
    reversed = {<<{data}};
  end
endmodule
