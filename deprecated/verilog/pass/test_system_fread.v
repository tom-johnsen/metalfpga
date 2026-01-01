// EXPECT=PASS
// Test: $fread binary input
// Feature: Read into reg and memory

module test_system_fread;
  integer fd;
  integer res;
  reg [15:0] rg;
  reg [7:0] mem [0:1];

  initial begin
    fd = $fopen("fread.txt", "w+");
    $fwrite(fd, "ab01");
    $rewind(fd);

    rg = 16'hxxxx;
    res = $fread(rg, fd);
    if (res != 2)
      $display("fread reg count mismatch: %0d", res);
    if (rg !== "ab")
      $display("fread reg mismatch: %h", rg);

    res = $fread(mem, fd, 0, 2);
    if (res != 2)
      $display("fread mem count mismatch: %0d", res);
    if (mem[0] !== "0")
      $display("fread mem[0] mismatch: %h", mem[0]);
    if (mem[1] !== "1")
      $display("fread mem[1] mismatch: %h", mem[1]);

    $fclose(fd);
  end
endmodule
