// EXPECT=PASS
// Test: Bidirectional port with tristate
// Feature: inout ports with tristate control
// Expected: Should fail - inout tristate not yet implemented

module tristate_driver(
  inout wire bus,
  input [7:0] data_out,
  output reg [7:0] data_in,
  input output_enable
);
  // Drive bus when output_enable is high
  assign bus = output_enable ? data_out : 8'bz;

  // Read from bus
  always @(bus) begin
    data_in = bus;
  end
endmodule

module test_tristate_bidirectional;
  wire [7:0] shared_bus;
  reg [7:0] data_a, data_b;
  wire [7:0] read_a, read_b;
  reg oe_a, oe_b;

  tristate_driver drv_a(
    .bus(shared_bus),
    .data_out(data_a),
    .data_in(read_a),
    .output_enable(oe_a)
  );

  tristate_driver drv_b(
    .bus(shared_bus),
    .data_out(data_b),
    .data_in(read_b),
    .output_enable(oe_b)
  );

  initial begin
    data_a = 8'hAA;
    data_b = 8'h55;
    oe_a = 1;
    oe_b = 0;
  end
endmodule
