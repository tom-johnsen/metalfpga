// Test initial blocks for initialization
module initial_test(
    output reg [7:0] initialized_value
);
    initial begin
        initialized_value = 8'h42;
    end
endmodule
