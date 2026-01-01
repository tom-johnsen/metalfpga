// EXPECT=PASS
// Test: $ungetc push-back
// Feature: Put character back on stream

module test_system_ungetc;
  integer fd;
  integer ch0;
  integer ch1;
  integer ret;

  initial begin
    fd = $fopen("ungetc.txt", "w+");
    $fwrite(fd, "ab");
    $rewind(fd);

    ch0 = $fgetc(fd);
    ret = $ungetc(ch0, fd);
    ch1 = $fgetc(fd);

    if (ch0 != ch1)
      $display("ungetc mismatch: %0d %0d", ch0, ch1);
    if (ret != ch0)
      $display("ungetc return mismatch: %0d %0d", ret, ch0);
    $fclose(fd);
  end
endmodule
