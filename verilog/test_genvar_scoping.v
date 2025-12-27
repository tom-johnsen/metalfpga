// EXPECT=PASS
// Test genvar scoping in nested generates
module test_genvar_scoping;
    wire [7:0] outer_result;
    wire [3:0] inner_result [0:1];
    reg [7:0] data;

    genvar i, j;

    // Outer loop uses genvar i
    generate
        for (i = 0; i < 8; i = i + 1) begin : outer_loop
            not (outer_result[i], data[i]);
        end
    endgenerate

    // Separate generate block can reuse genvar i (different scope)
    generate
        for (i = 0; i < 2; i = i + 1) begin : another_outer
            // Inner loop uses genvar j
            for (j = 0; j < 4; j = j + 1) begin : inner_loop
                buf (inner_result[i][j], data[j]);
            end
        end
    endgenerate

    initial begin
        data = 8'b11001010;

        #1 begin
            // Check outer loop result
            if (outer_result == 8'b00110101)
                $display("PASS: Outer genvar i scoping correct");
            else
                $display("FAIL: outer_result=%b", outer_result);

            // Check inner loop results
            if (inner_result[0] == 4'b1010 && inner_result[1] == 4'b1010)
                $display("PASS: Nested genvar i,j scoping correct");
            else
                $display("FAIL: inner results incorrect");
        end

        $finish;
    end
endmodule
