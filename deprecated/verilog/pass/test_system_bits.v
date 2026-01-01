// EXPECT=PASS
// Test: $bits system function (SystemVerilog)
// Feature: Return bit width of expression or type

module test_system_bits;
  reg [7:0] byte_val;
  reg [31:0] word_val;
  integer width;

  initial begin
    width = $bits(byte_val);  // 8
    width = $bits(word_val);  // 32
    width = $bits(byte_val + word_val);  // 32 (result width)
  end
endmodule
