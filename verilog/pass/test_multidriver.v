module multi_driver(
  input  wire a,
  input  wire b,
  output wire y
);
  assign y = a;
  assign y = b;
endmodule
