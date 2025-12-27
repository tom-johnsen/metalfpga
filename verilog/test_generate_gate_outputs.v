// EXPECT=PASS
// Test gate primitive outputs in generate blocks with genvar-based selects
module test_generate_gate_outputs;
    reg [7:0] data_in;
    wire [7:0] inv_out;
    wire [7:0] buf_out;

    genvar i;

    // Generate 8 inverters with genvar-based output select
    generate
        for (i = 0; i < 8; i = i + 1) begin : inv_array
            not (inv_out[i], data_in[i]);
        end
    endgenerate

    // Generate 8 buffers with genvar-based output select
    generate
        for (i = 0; i < 8; i = i + 1) begin : buf_array
            buf (buf_out[i], data_in[i]);
        end
    endgenerate

    initial begin
        data_in = 8'b10101010;

        #1 begin
            if (inv_out == 8'b01010101)
                $display("PASS: Generate inverter array with genvar output select");
            else
                $display("FAIL: inv_out=%b (expected 01010101)", inv_out);

            if (buf_out == 8'b10101010)
                $display("PASS: Generate buffer array with genvar output select");
            else
                $display("FAIL: buf_out=%b (expected 10101010)", buf_out);
        end

        $finish;
    end
endmodule
