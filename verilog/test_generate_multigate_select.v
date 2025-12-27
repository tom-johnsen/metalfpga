// EXPECT=PASS
// Test multiple gate outputs on same wire in generate
module test_generate_multigate_select;
    reg [3:0] a, b;
    wire [3:0] and_out, or_out, xor_out;

    genvar i;

    // Generate AND, OR, XOR gates for each bit
    generate
        for (i = 0; i < 4; i = i + 1) begin : gate_gen
            and (and_out[i], a[i], b[i]);
            or  (or_out[i],  a[i], b[i]);
            xor (xor_out[i], a[i], b[i]);
        end
    endgenerate

    initial begin
        a = 4'b1100;
        b = 4'b1010;

        #1 begin
            if (and_out == 4'b1000)
                $display("PASS: AND gate array with genvar select");
            else
                $display("FAIL: and_out=%b (expected 1000)", and_out);

            if (or_out == 4'b1110)
                $display("PASS: OR gate array with genvar select");
            else
                $display("FAIL: or_out=%b (expected 1110)", or_out);

            if (xor_out == 4'b0110)
                $display("PASS: XOR gate array with genvar select");
            else
                $display("FAIL: xor_out=%b (expected 0110)", xor_out);
        end

        $finish;
    end
endmodule
