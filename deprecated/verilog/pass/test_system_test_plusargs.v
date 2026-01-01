// EXPECT=PASS
// Test: $test$plusargs system function
// Feature: Check for command-line arguments

module test_system_test_plusargs;
  initial begin
    if ($test$plusargs("DEBUG"))
      $display("Debug mode enabled");

    if ($test$plusargs("VERBOSE"))
      $display("Verbose output enabled");
  end
endmodule
