// Test: Task with output and inout parameters
// Feature: Task parameter directions
// Expected: May fail - task output/inout parameters

module test_task_output;
  task swap;
    inout [7:0] a, b;
    reg [7:0] temp;
    begin
      temp = a;
      a = b;
      b = temp;
    end
  endtask

  task compute;
    input [7:0] x, y;
    output [7:0] sum, diff;
    begin
      sum = x + y;
      diff = x - y;
    end
  endtask

  reg [7:0] p, q, s, d;

  initial begin
    p = 8'h10;
    q = 8'h20;
    swap(p, q);
    compute(p, q, s, d);
  end
endmodule
