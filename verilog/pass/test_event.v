// Test: event data type and triggers
// Feature: Named events
// Expected: Should fail - events not yet implemented

module test_event;
  event data_ready;
  reg [7:0] data;

  initial begin
    @(data_ready);
    data = 8'hFF;
  end

  initial begin
    #10 -> data_ready;  // Trigger the event
  end
endmodule
