// Test: SystemVerilog priority if
// Feature: priority if statement
// Expected: Should fail - SystemVerilog construct

module test_priority_if;
  reg [7:0] data;
  reg [1:0] priority;

  always @* begin
    priority if (data > 200)
      priority = 2'b11;
    else if (data > 100)
      priority = 2'b10;
    else if (data > 50)
      priority = 2'b01;
    else
      priority = 2'b00;
  end
endmodule
