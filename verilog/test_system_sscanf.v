// EXPECT=PASS
// Test: $sscanf system function
// Feature: Parse string with format specifiers

module test_system_sscanf;
  reg [8*20:1] str;
  integer value;
  integer count;

  initial begin
    str = "FF";
    count = $sscanf(str, "%h", value);
    if (count == 1)
      $display("Parsed value: %d", value);
  end
endmodule
