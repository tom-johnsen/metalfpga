// Test: SystemVerilog foreach loop
// Feature: foreach array iteration
// Expected: Should fail - SystemVerilog construct

module test_foreach;
  int array[10];
  int sum;

  initial begin
    foreach (array[i]) begin
      array[i] = i * 2;
    end

    sum = 0;
    foreach (array[i]) begin
      sum = sum + array[i];
    end
  end
endmodule
