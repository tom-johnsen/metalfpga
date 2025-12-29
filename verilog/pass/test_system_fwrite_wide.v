// EXPECT=PASS
// Test: $fdisplay/$fwrite with wide values
module test_system_fwrite_wide;
  reg [127:0] data;
  integer fd;

  initial begin
    data = 128'h0123456789abcdef0011223344556677;
    fd = $fopen("wide_fwrite.txt", "w");
    $fdisplay(fd, "WIDE=%h", data);
    $fwrite(fd, "WIDE2=%h", data);
    $fclose(fd);
    $finish;
  end
endmodule
