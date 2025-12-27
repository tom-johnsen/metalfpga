// EXPECT=PASS
// Test gate outputs in conditional generate blocks
module test_generate_gate_conditional;
    parameter MODE = 1;
    reg [7:0] data;
    wire [7:0] result;

    genvar i;

    generate
        if (MODE == 0) begin : invert_mode
            for (i = 0; i < 8; i = i + 1) begin : gates
                not (result[i], data[i]);
            end
        end else begin : passthrough_mode
            for (i = 0; i < 8; i = i + 1) begin : gates
                buf (result[i], data[i]);
            end
        end
    endgenerate

    initial begin
        data = 8'b11001100;

        #1 begin
            // MODE=1: passthrough
            if (result == 8'b11001100)
                $display("PASS: Gate output in conditional generate (passthrough)");
            else
                $display("FAIL: result=%b (expected 11001100)", result);
        end

        $finish;
    end
endmodule
