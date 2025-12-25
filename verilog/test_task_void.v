// Test: Task with no return value (void task)
// Feature: tasks
// Expected: Should fail - tasks not yet implemented

module test_task_void;
  reg [7:0] counter;

  task increment_counter;
    begin
      counter = counter + 1;
    end
  endtask

  initial begin
    counter = 0;
    increment_counter;
    increment_counter;
    increment_counter;
  end
endmodule
