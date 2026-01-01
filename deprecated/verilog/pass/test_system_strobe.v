// EXPECT=PASS
// Test: $strobe system task
// Feature: Display at end of current time step (after NBA)
// Expected: Should fail - $strobe not yet implemented

module test_system_strobe;
  reg [7:0] data;

  initial begin
    data = 8'h00;
    $display("Display: data = %h", data);  // Shows 0x00
    $strobe("Strobe: data = %h", data);    // Shows 0xFF (after NBA)
    data <= 8'hFF;  // Non-blocking
  end
endmodule
