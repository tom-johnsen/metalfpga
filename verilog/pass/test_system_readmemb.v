// Test: $readmemb system task
// Feature: Load memory from binary file
// Expected: Should fail - $readmemb not yet implemented

module test_system_readmemb;
  reg [7:0] memory [0:255];

  initial begin
    $readmemb("data.bin", memory);           // Load entire array
    $readmemb("data.bin", memory, 10);       // Start at index 10
    $readmemb("data.bin", memory, 10, 20);   // Load indices 10-20
  end
endmodule
