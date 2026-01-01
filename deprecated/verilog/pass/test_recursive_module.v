// EXPECT=PASS
module recur(input a, output b);
  recur r(.a(b), .b(a));  // infinite loop!
endmodule
