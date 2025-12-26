// Test: SystemVerilog assertions
// Feature: Immediate and concurrent assertions
// Expected: Should fail - SystemVerilog construct

module test_assertion;
  reg clk;
  reg [7:0] data;

  // Immediate assertion
  initial begin
    assert (data < 8'hFF) else $error("Data overflow");
  end

  // Concurrent assertion
  property data_range;
    @(posedge clk) (data >= 0) && (data <= 100);
  endproperty

  assert property (data_range);
endmodule
