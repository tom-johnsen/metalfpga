module test_file_io_words;
  integer fd;
  integer fdw;
  integer ch;
  integer count;
  integer rc;
  integer pos;
  integer err;
  integer scan_a;
  integer scan_b;
  integer scan_rc;
  integer scan2_rc;
  integer plus_rc;
  integer plus_val;
  integer bytes;
  reg [8*32-1:0] line;
  reg [8*8-1:0] word;
  reg [7:0] mem [0:15];
  reg [7:0] memh [0:3];
  reg [7:0] memb [0:3];

  initial begin
    plus_val = 0;
    plus_rc = $value$plusargs("VAL=%d", plus_val);
    if ($test$plusargs("IO_TEST")) begin
      $display("plusargs IO_TEST set");
    end else begin
      $display("plusargs IO_TEST missing");
    end
    $display("plus_rc=%0d plus_val=%0d", plus_rc, plus_val);

    fd = $fopen("verilog/pass/file_io_words.txt", "r");
    if (fd == 0) begin
      $display("open failed");
      $finish;
    end
    fdw = $fopen("verilog/pass/file_io_words_out.txt", "w");
    if (fdw == 0) begin
      $display("open write failed");
      $finish;
    end

    $fdisplay(fdw, "line1 %0d", 42);
    $fwrite(fdw, "line2 %0h", 8'h2a);
    $fflush(fdw);
    $fflush;

    if ($feof(fd)) begin
      $display("empty file");
    end

    count = 0;
    while (!$feof(fd)) begin
      ch = $fgetc(fd);
      if (ch != 32'hFFFFFFFF) begin
        count = count + 1;
      end
    end
    $display("count=%0d", count);

    $rewind(fd);
    rc = $fseek(fd, 0, 0);
    pos = $ftell(fd);
    $display("seek rc=%0d pos=%0d", rc, pos);

    rc = $fgets(line, fd);
    $display("fgets rc=%0d", rc);

    rc = $fseek(fd, 0, 0);
    scan_rc = $fscanf(fd, "%d %h %s", scan_a, scan_b, word);
    $display("fscanf rc=%0d a=%0d b=%0h", scan_rc, scan_a, scan_b);

    ch = $fgetc(fd);
    rc = $ungetc(ch, fd);
    $display("ungetc rc=%0d", rc);

    bytes = $fread(mem, fd, 0, 4);
    $display("fread bytes=%0d", bytes);

    err = $ferror(fd);
    $display("ferror=%0d", err);

    scan2_rc = $sscanf("7 0x2A", "%d %h", scan_a, scan_b);
    $display("sscanf rc=%0d a=%0d b=%0h", scan2_rc, scan_a, scan_b);

    $readmemh("verilog/pass/file_io_words_hex.mem", memh);
    $readmemb("verilog/pass/file_io_words_bin.mem", memb);
    $writememh("verilog/pass/file_io_words_hex_out.mem", memh);
    $writememb("verilog/pass/file_io_words_bin_out.mem", memb);

    $fclose(fdw);
    $fclose(fd);
    $finish;
  end
endmodule
