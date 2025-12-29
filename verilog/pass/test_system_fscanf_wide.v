// EXPECT=PASS
// Test: $fscanf into wide register
module test_system_fscanf_wide;
  integer fd;
  reg [127:0] wide;
  integer count;

  initial begin
    fd = $fopen("wide_scan.txt", "w");
    $fwrite(fd, "deadbeefdeadbeefcafebabecafebabe\n");
    $fclose(fd);

    fd = $fopen("wide_scan.txt", "r");
    count = $fscanf(fd, "%h", wide);
    $display("COUNT=%0d WIDE=%h", count, wide);
    $fclose(fd);
    $finish;
  end
endmodule
