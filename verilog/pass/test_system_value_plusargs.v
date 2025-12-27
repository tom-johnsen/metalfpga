// Test: $value$plusargs system function
// Feature: Get value from command-line arguments
// Expected: Should fail - $value$plusargs not yet implemented

module test_system_value_plusargs;
  integer seed;
  reg [8*32:1] filename;

  initial begin
    if ($value$plusargs("SEED=%d", seed))
      $display("Seed = %d", seed);

    if ($value$plusargs("FILE=%s", filename))
      $display("Filename = %s", filename);
  end
endmodule
