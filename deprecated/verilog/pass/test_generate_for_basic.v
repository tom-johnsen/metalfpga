// EXPECT=PASS
// Test basic generate for loops
module test_generate_for_basic;
    wire [7:0] data_in;
    wire [7:0] data_out;

    assign data_in = 8'b10110011;

    // Generate 8 inverters using for loop
    genvar i;
    generate
        for (i = 0; i < 8; i = i + 1) begin : inv_array
            not (data_out[i], data_in[i]);
        end
    endgenerate

    initial begin
        #1 begin
            // data_out should be bitwise NOT of data_in
            if (data_out == 8'b01001100)
                $display("PASS: Generate for created 8 inverters correctly");
            else
                $display("FAIL: data_out=%b (expected 01001100)", data_out);
        end

        $finish;
    end
endmodule
