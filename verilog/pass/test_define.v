// EXPECT=PASS
// Test: define directive
// Feature: Compiler directives (preprocessor)
// Expected: Should fail - define not yet implemented

`define WIDTH 8
`define MAX_COUNT 255

module test_define;
  reg [`WIDTH-1:0] counter;
  reg [`WIDTH-1:0] max_val;

  initial begin
    counter = 0;
    max_val = `MAX_COUNT;
  end
endmodule
