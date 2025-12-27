// EXPECT=PASS
// Test: Hierarchical name references
// Feature: Cross-module signal access using dotted names

module sub_module;
  reg [7:0] internal_reg;

  initial begin
    internal_reg = 8'h42;
  end
endmodule

module test_hierarchical_name;
  sub_module sub();
  reg [7:0] copy;

  initial begin
    #10;
    copy = sub.internal_reg;  // Hierarchical reference
  end
endmodule
