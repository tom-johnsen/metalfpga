// Test: SystemVerilog queues
// Feature: Queue data type and methods
// Expected: Should fail - SystemVerilog construct

module test_queue;
  int q[$];
  int value;

  initial begin
    q.push_back(10);
    q.push_front(5);
    value = q.pop_back();
    value = q.size();
  end
endmodule
