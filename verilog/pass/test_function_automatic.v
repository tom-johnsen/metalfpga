// Test: Automatic functions (reentrant)
// Feature: automatic keyword for function storage
// Expected: Should fail - automatic not yet implemented

module test_function_automatic;
  function automatic [31:0] fibonacci;
    input [31:0] n;
    begin
      if (n <= 1)
        fibonacci = n;
      else
        fibonacci = fibonacci(n-1) + fibonacci(n-2);
    end
  endfunction

  reg [31:0] result;

  initial begin
    result = fibonacci(7);
  end
endmodule
