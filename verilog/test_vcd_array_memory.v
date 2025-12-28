// EXPECT=PASS
// Memory array operations - tests VCD dumping of array elements
module test_vcd_array_memory;
  reg clk;
  reg [7:0] mem [0:7];
  reg [2:0] write_addr;
  reg [2:0] read_addr;
  reg [7:0] write_data;
  reg [7:0] read_data;

  initial begin
    $dumpfile("array_memory.vcd");
    $dumpvars(0, test_vcd_array_memory);
    $dumpvars(0, mem[0], mem[1], mem[2], mem[3]);
    $dumpvars(0, mem[4], mem[5], mem[6], mem[7]);

    clk = 0;
    write_addr = 0;
    read_addr = 0;
    write_data = 8'hAA;

    // Initialize memory
    mem[0] = 8'h00;
    mem[1] = 8'h11;
    mem[2] = 8'h22;
    mem[3] = 8'h33;
    mem[4] = 8'h44;
    mem[5] = 8'h55;
    mem[6] = 8'h66;
    mem[7] = 8'h77;

    #16 $finish;
  end

  always #1 clk = ~clk;

  always @(posedge clk) begin
    mem[write_addr] <= write_data;
    read_data <= mem[read_addr];
    write_addr <= write_addr + 1;
    read_addr <= read_addr + 1;
    write_data <= write_data + 8'h11;
  end
endmodule
