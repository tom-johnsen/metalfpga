// Test memory read operations
module test_memory_read(
    input [7:0] rd_addr1,
    input [7:0] rd_addr2,
    input [7:0] rd_addr3,
    output reg [7:0] rd_data1,
    output reg [7:0] rd_data2,
    output reg [7:0] rd_data3
);
    // Memory array
    reg [7:0] mem [0:255];

    // Multiple combinational reads from same memory
    always @(*) begin
        rd_data1 = mem[rd_addr1];
        rd_data2 = mem[rd_addr2];
        rd_data3 = mem[rd_addr3];
    end
endmodule

// Test synchronous memory read (registered output)
module test_memory_read_sync(
    input clk,
    input [7:0] rd_addr,
    output reg [7:0] rd_data
);
    reg [7:0] mem [0:255];

    // Synchronous read - data appears on next clock
    always @(posedge clk) begin
        rd_data <= mem[rd_addr];
    end
endmodule

// Test memory read with different array sizes
module test_memory_read_sizes(
    input [3:0] small_addr,
    input [9:0] large_addr,
    input [4:0] wide_addr,
    output reg [7:0] small_data,
    output reg [7:0] large_data,
    output reg [15:0] wide_data
);
    reg [7:0] small_mem [0:15];      // 16 entries
    reg [7:0] large_mem [0:1023];    // 1024 entries
    reg [15:0] wide_mem [0:31];      // 32 entries, 16-bit wide

    always @(*) begin
        small_data = small_mem[small_addr];
        large_data = large_mem[large_addr];
        wide_data = wide_mem[wide_addr];
    end
endmodule
