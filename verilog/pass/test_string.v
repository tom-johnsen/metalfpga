// Test: String literals and reg arrays as strings
// Feature: String handling in Verilog
// Expected: Should fail - strings not yet implemented

module test_string;
  reg [8*12:1] message;

  initial begin
    message = "Hello World!";
    if (message == "Hello World!")
      message = "Test Passed";
  end
endmodule
