// EXPECT=PASS
// Test: wait statement
// Feature: Level-sensitive wait
// Expected: Should fail - wait not yet implemented

module test_wait;
  reg enable;
  reg [7:0] data;

  initial begin
    data = 0;
    wait (enable);  // Wait until enable is true
    data = 8'hFF;
  end

  initial begin
    enable = 0;
    #10 enable = 1;
  end
endmodule
