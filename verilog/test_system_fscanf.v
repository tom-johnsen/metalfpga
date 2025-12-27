// EXPECT=PASS
// Test: $fscanf file input
// Feature: Reading formatted data from files

module test_system_fscanf;
  integer fd;
  integer count;
  reg [7:0] value;

  initial begin
    fd = $fopen("input.txt", "r");

    count = $fscanf(fd, "%h", value);
    if (count != 1)
      $display("Read failed");

    $fclose(fd);
  end
endmodule
