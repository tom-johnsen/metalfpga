// EXPECT=PASS
// Test signed arithmetic in procedural blocks
module test_signed_in_always(
    input clk,
    input signed [7:0] a,
    input signed [7:0] b,
    output reg signed [7:0] sum,
    output reg signed [7:0] diff,
    output reg signed [15:0] prod,
    output reg is_negative,
    output reg is_positive,
    output reg [1:0] comparison
);
    always @(posedge clk) begin
        sum <= a + b;
        diff <= a - b;
        prod <= a * b;

        is_negative <= (a < 0);
        is_positive <= (a > 0);

        // Signed comparison in conditional
        if (a < b) begin
            comparison <= 2'b01;
        end else if (a > b) begin
            comparison <= 2'b10;
        end else begin
            comparison <= 2'b00;
        end
    end
endmodule
