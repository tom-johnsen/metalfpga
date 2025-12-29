// EXPECT=PASS
// Test: $fseek file reposition
// Feature: Seek within file and read back

module test_system_fseek;
  integer fd;
  integer rc;
  integer ch;

  initial begin
    fd = $fopen("seek.txt", "w+");
    $fwrite(fd, "abcdef");
    rc = $fseek(fd, 2, 0);
    ch = $fgetc(fd);
    if (rc != 0)
      $display("fseek failed: %0d", rc);
    if (ch != 8'h63)
      $display("fseek read mismatch: %0d", ch);
    $fclose(fd);
  end
endmodule
