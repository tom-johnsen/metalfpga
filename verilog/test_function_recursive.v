// EXPECT=PASS
// Test: Recursive function
// Feature: Function calling itself

module test_function_recursive;
  function [31:0] factorial;
    input [31:0] n;
    begin
      if (n <= 1)
        factorial = 1;
      else
        factorial = n * factorial(n - 1);
    end
  endfunction

  reg [31:0] result;

  initial begin
    result = factorial(5);  // Should be 120
  end
endmodule
