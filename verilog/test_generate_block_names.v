// EXPECT=PASS
// Test generate block naming and hierarchical access
module test_generate_block_names;
    wire [3:0] result;
    reg [3:0] data;

    genvar i;

    generate
        for (i = 0; i < 4; i = i + 1) begin : named_block
            // Each iteration creates named_block[0], named_block[1], etc.
            wire temp;
            not (temp, data[i]);
            buf (result[i], temp);
        end
    endgenerate

    initial begin
        data = 4'b1100;

        #1 begin
            if (result == 4'b0011)
                $display("PASS: Generate block naming works");
            else
                $display("FAIL: result=%b (expected 0011)", result);

            // Hierarchical reference to generated instance
            // named_block[0].temp should be 0 (NOT of data[0]=0)
            if (named_block[0].temp == 1'b0)
                $display("PASS: Hierarchical access to generate block");
            else
                $display("FAIL: named_block[0].temp=%b", named_block[0].temp);
        end

        $finish;
    end
endmodule
