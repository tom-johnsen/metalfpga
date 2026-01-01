// EXPECT=PASS
// Test: $readmemh with wide memory elements (>64 bits)
// Feature: Wide read + dump back out via $writememh

module test_system_readmemh_wide;
  reg [127:0] mem [0:3];

  initial begin
    $readmemh("verilog/pass/wide_readmem.hex", mem);
    $writememh("wide_readmem_out.hex", mem);
    $finish;
  end
endmodule
