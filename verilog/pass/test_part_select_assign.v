// Test: procedural part-select assignment (fixed and indexed)
// Feature: assign to [msb:lsb], [idx +: w], [idx -: w]
module test_part_select_assign;
  reg [15:0] data;
  integer idx;

  initial begin
    data = 16'h0000;
    data[7:0] = 8'hAA;
    data[15:8] = 8'h55;
    idx = 4;
    data[idx +: 4] = 4'hF;
    data[idx -: 4] = 4'h0;
  end
endmodule
