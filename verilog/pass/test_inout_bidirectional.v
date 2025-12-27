// EXPECT=PASS
// Test: Bidirectional inout ports with tristate control
// Feature: inout port with conditional drive
// Expected: May fail - complex inout handling

module bidir_buffer (
  inout wire data,
  input wire drive_enable,
  input wire data_out,
  output reg data_in
);
  assign data = drive_enable ? data_out : 1'bz;

  always @(data) begin
    data_in = data;
  end
endmodule

module test_inout_bidirectional;
  wire shared;
  reg en1, en2, d1, d2;
  wire out1, out2;

  bidir_buffer buf1 (.data(shared), .drive_enable(en1), .data_out(d1), .data_in(out1));
  bidir_buffer buf2 (.data(shared), .drive_enable(en2), .data_out(d2), .data_in(out2));

  initial begin
    en1 = 1; en2 = 0; d1 = 1;
    #10 en1 = 0; en2 = 1; d2 = 0;
  end
endmodule
