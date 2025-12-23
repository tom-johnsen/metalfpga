module gen_test;
  generate
    genvar i;
    for (i = 0; i < 4; i = i + 1) begin
      wire [7:0] data;
    end
  endgenerate
endmodule
