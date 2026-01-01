// Test: SystemVerilog struct
// Feature: Structure data types
// Expected: Should fail - SystemVerilog construct

module test_struct;
  typedef struct {
    reg [7:0] data;
    reg valid;
    reg [3:0] id;
  } packet_t;

  packet_t tx_packet, rx_packet;

  initial begin
    tx_packet.data = 8'hAB;
    tx_packet.valid = 1'b1;
    tx_packet.id = 4'h5;
  end
endmodule
