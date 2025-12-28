// EXPECT=PASS
// Deep module hierarchy - tests VCD scope nesting
module leaf_module(input clk, output reg [3:0] data);
  always @(posedge clk) begin
    data = data + 1;
  end

  initial data = 0;
endmodule

module middle_module(input clk, output [3:0] leaf_data);
  reg [7:0] middle_counter;

  leaf_module leaf_inst (
    .clk(clk),
    .data(leaf_data)
  );

  always @(posedge clk) begin
    middle_counter = middle_counter + 2;
  end

  initial middle_counter = 0;
endmodule

module test_vcd_hierarchy_deep;
  reg clk;
  wire [3:0] data_out;
  reg [15:0] top_counter;

  middle_module mid_inst (
    .clk(clk),
    .leaf_data(data_out)
  );

  initial begin
    $dumpfile("hierarchy_deep.vcd");
    $dumpvars(0, test_vcd_hierarchy_deep);

    clk = 0;
    top_counter = 0;

    #12 $finish;
  end

  always #1 clk = ~clk;

  always @(posedge clk) begin
    top_counter = top_counter + 1;
  end
endmodule
