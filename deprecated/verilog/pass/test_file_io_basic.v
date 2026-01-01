module test_file_io_basic;
  integer fd;
  integer ch;
  integer count;
  integer sum;
  integer rc;

  initial begin
    fd = $fopen("verilog/pass/file_io_basic.txt", "r");
    if (fd == 0) begin
      $display("open failed");
      $finish;
    end

    if ($feof(fd)) begin
      $display("empty file");
    end else begin
      $display("file not empty");
    end

    count = 0;
    sum = 0;
    while (!$feof(fd)) begin
      ch = $fgetc(fd);
      if (ch != 32'hFFFFFFFF) begin
        sum = sum + ch;
        count = count + 1;
      end
    end

    rc = $fseek(fd, 0, 0);
    $display("count=%0d sum=%0d rc=%0d", count, sum, rc);
    $fclose(fd);
    $finish;
  end
endmodule
