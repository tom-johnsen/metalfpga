// Test reduction operators in case statements
module test_reduction_case(
    input [7:0] data,
    output reg [2:0] category,
    output reg all_bits,
    output reg any_bit
);
    always @(*) begin
        all_bits = &data;
        any_bit = |data;

        case ({&data, |data})
            2'b11: category = 3'b001;  // All ones
            2'b01: category = 3'b010;  // Some ones
            2'b00: category = 3'b100;  // All zeros
            default: category = 3'b000;
        endcase
    end
endmodule
