// EXPECT=PASS
// Test: Wide bit/range select writes (dynamic indices)
// Feature: Indexed part-select and bit-select on >64-bit vector

module test_wide_select_write;
  reg [127:0] data;
  reg [7:0] low;
  reg [7:0] slice;
  reg bit_val;
  integer idx;

  initial begin
    data = 128'h0;
    data[7:0] = 8'h3c;
    data[127:120] = 8'hff;

    idx = 16;
    data[idx +: 8] = 8'ha5;
    slice = data[idx +: 8];
    low = data[7:0];

    idx = 100;
    data[idx] = 1'b1;
    bit_val = data[idx];

    $display("LOW=%h SLICE=%h MSB=%h BIT=%b", low, slice, data[127:120], bit_val);
    $finish;
  end
endmodule
