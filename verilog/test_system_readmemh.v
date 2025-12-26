// Test: $readmemh system task
// Feature: Load memory from hex file
// Expected: Should fail - $readmemh not yet implemented

module test_system_readmemh;
  reg [31:0] rom [0:1023];

  initial begin
    $readmemh("program.hex", rom);
  end
endmodule
