// EXPECT=PASS
// Test: $ferror status reporting
// Feature: Error query on valid handle

module test_system_ferror;
  integer fd;
  integer err;

  initial begin
    fd = $fopen("ferror.txt", "w+");
    $fwrite(fd, "ok");
    err = $ferror(fd);
    if (err != 0)
      $display("ferror unexpected: %0d", err);
    $fclose(fd);
  end
endmodule
