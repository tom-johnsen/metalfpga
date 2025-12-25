// Test: force and release statements
// Feature: Procedural force/release
// Expected: Should fail - force/release not yet implemented

module test_force_release;
  reg [7:0] data;
  reg clk;

  always @(posedge clk) begin
    data = data + 1;
  end

  initial begin
    clk = 0;
    force data = 8'hFF;  // Override normal behavior
    #10;
    release data;        // Return to normal behavior
  end
endmodule
