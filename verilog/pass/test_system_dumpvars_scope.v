// EXPECT=PASS
// Test: $dumpvars with depth control
// Feature: Selective variable dumping by hierarchy depth
// Expected: Should fail - $dumpvars not yet implemented

module sub_module;
  reg [7:0] internal_data;
endmodule

module test_system_dumpvars_scope;
  reg [7:0] top_data;
  sub_module sub();

  initial begin
    $dumpfile("selective.vcd");
    $dumpvars(1, test_system_dumpvars_scope);  // Depth 1: only top-level
    // $dumpvars(2, test_system_dumpvars_scope);  // Depth 2: top + 1 level down
    // $dumpvars;  // No args: dump everything
  end
endmodule
