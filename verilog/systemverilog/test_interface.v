// Test: SystemVerilog interface
// Feature: Interface declarations and connections
// Expected: Should fail - SystemVerilog construct

interface bus_if;
  logic [7:0] data;
  logic valid;
  logic ready;
endinterface

module test_interface;
  bus_if bus();

  initial begin
    bus.data = 8'hFF;
    bus.valid = 1'b1;
  end
endmodule
