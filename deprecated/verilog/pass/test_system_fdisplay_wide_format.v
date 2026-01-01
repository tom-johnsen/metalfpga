// EXPECT=PASS
// Test: $fdisplay/$fwrite width and zero-pad with wide values
module test_system_fdisplay_wide_format;
  reg [127:0] data;
  integer fd;

  initial begin
    data = 128'h1;
    fd = $fopen("wide_fmt.txt", "w");
    $fdisplay(fd, "ZPAD=%032h", data);
    $fdisplay(fd, "SPAD=%40h", data);
    $fwrite(fd, "RAW=%h", data);
    $fclose(fd);
    $finish;
  end
endmodule
