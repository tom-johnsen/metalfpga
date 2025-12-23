module func_test(input [7:0] a, output [7:0] y);
  function [7:0] double;
    input [7:0] x;
    double = x << 1;
  endfunction
  assign y = double(a);
endmodule
