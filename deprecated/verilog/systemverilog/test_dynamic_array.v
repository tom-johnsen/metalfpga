// Test: SystemVerilog dynamic arrays
// Feature: Dynamic array allocation with new[]
// Expected: Should fail - SystemVerilog construct

module test_dynamic_array;
  int dyn_array[];

  initial begin
    dyn_array = new[10];
    dyn_array[0] = 42;
    dyn_array = new[20](dyn_array);  // Resize and copy
  end
endmodule
