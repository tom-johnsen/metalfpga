// Test: $fwrite, $fdisplay file output
// Feature: Writing to files with formatting
// Expected: Should fail - file write not yet implemented

module test_system_fwrite;
  integer fd;
  reg [7:0] data;

  initial begin
    fd = $fopen("log.txt", "w");
    data = 8'hAB;

    $fdisplay(fd, "Data value: %h", data);
    $fwrite(fd, "No newline here");
    $fwrite(fd, " - continued\n");

    $fclose(fd);
  end
endmodule
