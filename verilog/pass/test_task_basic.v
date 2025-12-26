// Test: Basic task declaration and call
// Feature: tasks (procedural task blocks)
// Expected: Should fail - tasks not yet implemented

module test_task_basic;
  reg [7:0] result;
  reg [7:0] a, b;

  // Task to add two numbers
  task add_numbers;
    input [7:0] x;
    input [7:0] y;
    output [7:0] sum;
    begin
      sum = x + y;
    end
  endtask

  initial begin
    a = 8'd10;
    b = 8'd20;
    add_numbers(a, b, result);
  end
endmodule
