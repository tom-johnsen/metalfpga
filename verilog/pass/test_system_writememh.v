// EXPECT=PASS
// Test: $writememh system task
// Feature: Write memory to hex file
// Expected: Hex file is written with memory contents

module test_system_writememh;
  reg [15:0] data [0:15];
  integer i;

  initial begin
    for (i = 0; i < 16; i = i + 1)
      data[i] = i * 16'h1111;

    $writememh("output.hex", data);
  end
endmodule
