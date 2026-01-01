// Test: SystemVerilog associative arrays
// Feature: Associative array with string keys
// Expected: Should fail - SystemVerilog construct

module test_associative_array;
  int assoc[string];

  initial begin
    assoc["apple"] = 1;
    assoc["banana"] = 2;
    if (assoc.exists("apple"))
      assoc.delete("apple");
  end
endmodule
