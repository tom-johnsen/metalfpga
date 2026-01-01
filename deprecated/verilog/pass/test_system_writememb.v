// EXPECT=PASS
// Test: $writememb system task
// Feature: Write memory to binary file
// Expected: Binary files are written with memory contents

module test_system_writememb;
  reg [7:0] memory [0:255];
  integer i;

  initial begin
    for (i = 0; i < 256; i = i + 1)
      memory[i] = i;

    $writememb("output.bin", memory);
    $writememb("partial.bin", memory, 10, 20);  // Write subset
  end
endmodule
