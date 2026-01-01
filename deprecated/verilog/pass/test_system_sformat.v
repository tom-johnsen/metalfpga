// EXPECT=PASS
// Test: $sformat system task
// Feature: Format string into variable
// Expected: Should fail - $sformat not yet implemented

module test_system_sformat;
  reg [8*50:1] message;
  reg [7:0] data;

  initial begin
    data = 8'hAB;
    $sformat(message, "Data value is %h", data);
    $display("%s", message);
  end
endmodule
