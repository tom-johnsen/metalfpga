// EXPECT=PASS
// Test multiple always blocks with different clock edges
module mixed_edge(
    input clk,
    input rst,
    input [7:0] data,
    output reg [7:0] q_pos,
    output reg [7:0] q_neg
);
    always @(posedge clk) begin
        if (rst)
            q_pos <= 8'h00;
        else
            q_pos <= data;
    end

    always @(negedge clk) begin
        if (rst)
            q_neg <= 8'hFF;
        else
            q_neg <= data;
    end
endmodule
