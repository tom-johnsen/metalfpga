// EXPECT=PASS
// Test: $fflush task
// Feature: Flush file buffers

module test_system_fflush;
  integer fd;

  initial begin
    fd = $fopen("fflush.txt", "w");
    $fwrite(fd, "flush");
    $fflush(fd);
    $fclose(fd);
  end
endmodule
