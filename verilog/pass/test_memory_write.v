module mem_write_test(
    input clk,
    input [7:0] wr_addr,
    input [7:0] wr_data,
    input wr_en,
    input [7:0] rd_addr,
    output reg [7:0] rd_data
);
    reg [7:0] mem [0:255];

    // Write on clock edge when write enable is high
    always @(posedge clk) begin
        if (wr_en)
            mem[wr_addr] <= wr_data;
    end

    // Combinational read
    always @(*) begin
        rd_data = mem[rd_addr];
    end
endmodule
