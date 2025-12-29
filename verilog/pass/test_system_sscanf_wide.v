// EXPECT=PASS
// Test: $sscanf into wide register
module test_system_sscanf_wide;
  reg [8*32-1:0] input;
  reg [127:0] wide;
  integer count;

  initial begin
    input = "0123456789abcdef0011223344556677";
    count = $sscanf(input, "%h", wide);
    $display("COUNT=%0d WIDE=%h", count, wide);
    $finish;
  end
endmodule
