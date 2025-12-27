// EXPECT=PASS
// Test generate blocks with localparams
module test_generate_localparam;
    parameter MODE = 1;

    wire [7:0] result;
    reg [7:0] data;

    generate
        if (MODE == 0) begin : mode0
            localparam SHIFT = 1;
            assign result = data << SHIFT;
        end else if (MODE == 1) begin : mode1
            localparam SHIFT = 2;
            assign result = data << SHIFT;
        end else begin : mode_default
            localparam SHIFT = 3;
            assign result = data << SHIFT;
        end
    endgenerate

    initial begin
        data = 8'd5;  // Binary: 00000101

        #1 begin
            // MODE=1, SHIFT=2, so 5 << 2 = 20
            if (result == 8'd20)
                $display("PASS: Generate localparam SHIFT=2 applied");
            else
                $display("FAIL: result=%d (expected 20)", result);
        end

        $finish;
    end
endmodule
