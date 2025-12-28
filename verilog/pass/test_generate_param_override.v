// EXPECT=PASS
// Test parameter override in generated instances
module parameterized_gate #(parameter INVERT = 0) (
    input in,
    output out
);
    generate
        if (INVERT) begin : inv
            not (out, in);
        end else begin : buf
            buf (out, in);
        end
    endgenerate
endmodule

module test_generate_param_override;
    reg [3:0] data;
    wire [3:0] result;

    genvar i;

    generate
        for (i = 0; i < 4; i = i + 1) begin : gate_array
            // Even indices: buffer (INVERT=0)
            // Odd indices: inverter (INVERT=1)
            parameterized_gate #(.INVERT(i % 2)) u_gate (
                .in(data[i]),
                .out(result[i])
            );
        end
    endgenerate

    initial begin
        data = 4'b1010;

        #1 begin
            // Index 0,2: buffer -> 1,1
            // Index 1,3: invert -> 1,1
            // Result: 1111
            if (result == 4'b1111)
                $display("PASS: Parameter override in generate loop");
            else
                $display("FAIL: result=%b (expected 1111)", result);
        end

        #1 data = 4'b1100;

        #1 begin
            // Index 0: buf(1)=1, Index 1: not(1)=0
            // Index 2: buf(0)=0, Index 3: not(0)=1
            // Result: 1001
            if (result == 4'b1001)
                $display("PASS: Dynamic parameter in generate");
            else
                $display("FAIL: result=%b (expected 1001)", result);
        end

        $finish;
    end
endmodule
