// EXPECT=PASS
// Test: $feof end-of-file detection
// Feature: Check if at end of file

module test_system_feof;
  integer fd;
  integer ch;

  initial begin
    fd = $fopen("input.txt", "r");

    while (!$feof(fd)) begin
      ch = $fgetc(fd);
    end

    $fclose(fd);
  end
endmodule
