// EXPECT=PASS
// Test: Memory-style tristate bus
// Feature: Realistic tristate bus with address decode
// Expected: Should fail - tristate resolution not yet implemented

module memory_block(
  inout [7:0] data_bus,
  input [3:0] addr,
  input [3:0] my_addr,
  input write_en,
  input output_en
);
  reg [7:0] storage;
  wire selected;

  assign selected = (addr == my_addr);
  assign data_bus = (selected && output_en) ? storage : 8'bz;

  always @(*) begin
    if (selected && write_en)
      storage = data_bus;
  end
endmodule

module test_tristate_memory_bus;
  wire [7:0] shared_data;
  reg [3:0] address;
  reg wr_en, oe;

  memory_block mem0(shared_data, address, 4'd0, wr_en, oe);
  memory_block mem1(shared_data, address, 4'd1, wr_en, oe);
  memory_block mem2(shared_data, address, 4'd2, wr_en, oe);
  memory_block mem3(shared_data, address, 4'd3, wr_en, oe);

  initial begin
    address = 4'd0;
    wr_en = 0;
    oe = 1;
  end
endmodule
