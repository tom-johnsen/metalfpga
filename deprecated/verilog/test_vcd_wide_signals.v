// EXPECT=PASS
// Wide signals - tests VCD encoding of large bit vectors
module test_vcd_wide_signals;
  reg clk;
  reg [31:0] wide_32;
  reg [63:0] wide_64;
  reg [127:0] wide_128;

  initial begin
    $dumpfile("wide_signals.vcd");
    $dumpvars(0, test_vcd_wide_signals);

    clk = 0;
    wide_32 = 32'hDEADBEEF;
    wide_64 = 64'hCAFEBABE_DEADBEEF;
    wide_128 = 128'h0123456789ABCDEF_FEDCBA9876543210;

    #10 $finish;
  end

  always #1 clk = ~clk;

  always @(posedge clk) begin
    wide_32 <= {wide_32[30:0], wide_32[31]};    // Rotate
    wide_64 <= wide_64 + 64'h1111111111111111;  // Increment
    wide_128 <= {wide_128[126:0], wide_128[127]}; // Rotate
  end
endmodule
