// EXPECT=PASS
// Test posedge/negedge edge sensitivity with multiple transitions
module test_edge_sensitivity;
    reg clk;
    reg [7:0] posedge_count;
    reg [7:0] negedge_count;
    reg [7:0] anyedge_count;

    initial begin
        clk = 0;
        posedge_count = 0;
        negedge_count = 0;
        anyedge_count = 0;

        // Generate 5 clock cycles
        repeat(5) begin
            #5 clk = 1;
            #5 clk = 0;
        end

        #1 begin
            if (posedge_count == 8'd5)
                $display("PASS: Detected 5 posedges");
            else
                $display("FAIL: posedge_count=%d (expected 5)", posedge_count);

            if (negedge_count == 8'd5)
                $display("PASS: Detected 5 negedges");
            else
                $display("FAIL: negedge_count=%d (expected 5)", negedge_count);

            if (anyedge_count == 8'd10)
                $display("PASS: Detected 10 total edges");
            else
                $display("FAIL: anyedge_count=%d (expected 10)", anyedge_count);
        end

        $finish;
    end

    always @(posedge clk) begin
        posedge_count = posedge_count + 1;
        anyedge_count = anyedge_count + 1;
    end

    always @(negedge clk) begin
        negedge_count = negedge_count + 1;
        anyedge_count = anyedge_count + 1;
    end
endmodule
