// EXPECT=PASS
// 4-state logic with X and Z values - tests VCD 4-state encoding
module test_vcd_4state_xz;
  reg clk;
  reg [3:0] has_x;
  reg [3:0] has_z;
  reg [3:0] mixed;

  initial begin
    $dumpfile("4state_xz.vcd");
    $dumpvars(0, test_vcd_4state_xz);

    clk = 0;
    has_x = 4'bxxxx;
    has_z = 4'bzzzz;
    mixed = 4'b10xz;

    #8 $finish;
  end

  always #1 clk = ~clk;

  always @(posedge clk) begin
    has_x = {has_x[2:0], has_x[3]};  // Rotate X values
    has_z = {has_z[2:0], 1'b0};      // Replace Z with 0
    mixed = {mixed[2:0], mixed[3]};   // Rotate mixed
  end
endmodule
