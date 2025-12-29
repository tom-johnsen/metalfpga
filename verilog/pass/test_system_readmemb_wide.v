// EXPECT=PASS
// Test: $readmemb with wide memory elements (>64 bits)
module test_system_readmemb_wide;
  reg [127:0] mem [0:3];

  initial begin
    $readmemb("verilog/pass/wide_readmemb.bin", mem);
    $writememb("wide_readmemb_out.bin", mem);
    $finish;
  end
endmodule
