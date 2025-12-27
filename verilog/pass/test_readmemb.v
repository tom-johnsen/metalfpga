// EXPECT=PASS
// Test: $readmemb system task
// Feature: System tasks for memory initialization
// Expected: Should fail - $readmemb not yet implemented

module test_readmemb;
  reg [7:0] memory [0:255];

  initial begin
    $readmemb("data.bin", memory);
  end
endmodule
