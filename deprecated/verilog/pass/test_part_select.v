// EXPECT=PASS
// Test variable part selects and edge cases
module part_select(
    input [15:0] data,
    input [3:0] sel,
    output wire [7:0] upper_byte,
    output wire [7:0] lower_byte,
    output wire [3:0] nibble,
    output wire single_bit
);
    assign upper_byte = data[15:8];  // Fixed part select
    assign lower_byte = data[7:0];   // Fixed part select
    assign nibble = data[7:4];       // Fixed part select
    assign single_bit = data[sel];   // Variable bit select
endmodule
