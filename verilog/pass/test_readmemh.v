// Test: $readmemh system task
// Feature: System tasks for memory initialization
// Expected: Should fail - $readmemh not yet implemented

module test_readmemh;
  reg [7:0] memory [0:255];

  initial begin
    $readmemh("data.hex", memory);
  end
endmodule
