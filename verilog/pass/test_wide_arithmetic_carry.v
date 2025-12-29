// EXPECT=PASS
// Test: Wide arithmetic carry across 64-bit boundary
// Feature: 128-bit add/sub with carry/borrow

module test_wide_arithmetic_carry;
  reg [127:0] a;
  reg [127:0] b;
  reg [127:0] sum;
  reg [127:0] diff;

  initial begin
    a = 128'h0000000000000000ffffffffffffffff;
    b = 128'h00000000000000000000000000000001;
    sum = a + b;
    diff = sum - b;

    if (sum !== 128'h00000000000000010000000000000000) begin
      $display("FAIL: carry across 64-bit boundary");
    end else begin
      $display("PASS: carry across 64-bit boundary");
    end

    if (diff !== a) begin
      $display("FAIL: borrow across 64-bit boundary");
    end else begin
      $display("PASS: borrow across 64-bit boundary");
    end

    $finish;
  end
endmodule
