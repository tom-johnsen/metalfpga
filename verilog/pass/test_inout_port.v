// Test inout (bidirectional) ports
module inout_test(
    input en,
    input [7:0] data_out,
    inout [7:0] data_bus,
    output reg [7:0] data_in
);
    assign data_bus = en ? data_out : 8'bz;

    always @(*) begin
        data_in = data_bus;
    end
endmodule
