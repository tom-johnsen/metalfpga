// Test: $fgetc character input
// Feature: Read single character from file
// Expected: Should fail - $fgetc not yet implemented

module test_system_fgetc;
  integer fd;
  integer ch;

  initial begin
    fd = $fopen("text.txt", "r");

    ch = $fgetc(fd);
    while (ch != -1) begin  // -1 = EOF
      ch = $fgetc(fd);
    end

    $fclose(fd);
  end
endmodule
