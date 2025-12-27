// EXPECT=PASS
module nowhitespace(input wire[7:0]a,input wire[7:0]b,output wire[7:0]y);
assign y={a[3:0],b[7:4]};
endmodule
