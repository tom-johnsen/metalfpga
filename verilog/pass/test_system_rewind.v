// EXPECT=PASS
// Test: $rewind file position reset
// Feature: Rewind and subsequent read

module test_system_rewind;
  integer fd;
  integer pos_before;
  integer pos_after;
  integer count;
  reg [8*8:1] line;

  initial begin
    fd = $fopen("rewind.txt", "w+");
    $fwrite(fd, "abcd");
    pos_before = $ftell(fd);
    $rewind(fd);
    pos_after = $ftell(fd);
    count = $fgets(line, fd);
    $fclose(fd);

    if (pos_before != 4)
      $display("ftell before rewind mismatch: %0d", pos_before);
    if (pos_after != 0)
      $display("ftell after rewind mismatch: %0d", pos_after);
    if (count != 4)
      $display("fgets after rewind mismatch: %0d", count);
  end
endmodule
