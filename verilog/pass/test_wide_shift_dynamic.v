// EXPECT=PASS
// Test: Wide dynamic shifts (>=64 and variable)
// Feature: 128-bit shifts with large shift amounts

module test_wide_shift_dynamic;
  reg [127:0] val;
  reg [7:0] shift;
  reg [127:0] res_shl;
  reg [127:0] res_shr;

  initial begin
    val = 128'h00000000000000000000000000000001;
    shift = 8'd65;
    res_shl = val << shift;
    if (res_shl !== 128'h00000000000000020000000000000000) begin
      $display("FAIL: shift left >= 64");
    end else begin
      $display("PASS: shift left >= 64");
    end

    val = 128'h80000000000000000000000000000000;
    shift = 8'd64;
    res_shr = val >> shift;
    if (res_shr !== 128'h00000000000000008000000000000000) begin
      $display("FAIL: shift right >= 64");
    end else begin
      $display("PASS: shift right >= 64");
    end

    $finish;
  end
endmodule
