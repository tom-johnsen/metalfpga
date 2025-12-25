// Test reduction operators in procedural blocks
module test_reduction_in_always(
    input clk,
    input [7:0] data,
    output reg all_ones,
    output reg any_ones,
    output reg parity,
    output reg [2:0] counter
);
    always @(posedge clk) begin
        all_ones <= &data;
        any_ones <= |data;
        parity <= ^data;

        // Use reduction in condition
        if (&data) begin
            counter <= counter + 1;
        end else if (|data) begin
            counter <= counter;
        end else begin
            counter <= 3'b000;
        end
    end
endmodule
