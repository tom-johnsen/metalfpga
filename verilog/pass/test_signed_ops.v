// Test signed arithmetic
module signed_ops(
    input signed [7:0] a,
    input signed [7:0] b,
    output wire signed [7:0] sum,
    output wire signed [7:0] diff,
    output wire signed [15:0] product,
    output wire less_than,
    output wire greater_than
);
    assign sum = a + b;
    assign diff = a - b;
    assign product = a * b;
    assign less_than = a < b;    // Signed comparison
    assign greater_than = a > b; // Signed comparison
endmodule
