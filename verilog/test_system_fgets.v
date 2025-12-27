// EXPECT=PASS
// Test: $fgets line input
// Feature: Read line from file into string

module test_system_fgets;
  integer fd;
  integer code;
  reg [8*80:1] line;  // 80 character string

  initial begin
    fd = $fopen("text.txt", "r");

    code = $fgets(line, fd);
    while (code != 0) begin
      code = $fgets(line, fd);
    end

    $fclose(fd);
  end
endmodule
