// EXPECT=PASS
// Test: $sformat with wide values
module test_system_sformat_wide;
  reg [8*64-1:0] message;
  reg [127:0] data;

  initial begin
    data = 128'h0123456789abcdef0011223344556677;
    $sformat(message, "MSG=%h", data);
    $display("%s", message);
    $finish;
  end
endmodule
