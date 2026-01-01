// EXPECT=PASS
// Test: $ftell file position reporting
// Feature: File position reporting after writes

module test_system_ftell;
  integer fd;
  integer pos0;
  integer pos1;
  integer pos2;

  initial begin
    fd = $fopen("ftell.txt", "w");
    pos0 = $ftell(fd);
    $fwrite(fd, "abcd");
    pos1 = $ftell(fd);
    $fwrite(fd, "ef");
    pos2 = $ftell(fd);
    $fclose(fd);

    if (pos0 != 0)
      $display("ftell start mismatch: %0d", pos0);
    if (pos1 != 4)
      $display("ftell after 4 bytes mismatch: %0d", pos1);
    if (pos2 != 6)
      $display("ftell after 6 bytes mismatch: %0d", pos2);
  end
endmodule
