// EXPECT=PASS
// Test that NBA prevents race conditions between always blocks
module test_nba_race_condition;
    reg clk;
    reg [7:0] data;
    reg [7:0] copy1, copy2;

    initial begin
        clk = 0;
        data = 8'd50;
        copy1 = 0;
        copy2 = 0;

        #1 clk = 1;
        #1 begin
            // Both always blocks should see OLD value of data (50)
            // Both copies should be 50, not dependent on execution order
            if (copy1 == 8'd50 && copy2 == 8'd50)
                $display("PASS: NBA prevented race - both got old value");
            else
                $display("FAIL: copy1=%d copy2=%d (expected 50,50)", copy1, copy2);

            // data itself should be updated to 100
            if (data == 8'd100)
                $display("PASS: data updated to 100");
            else
                $display("FAIL: data=%d (expected 100)", data);
        end

        #1 clk = 0;
        #1 clk = 1;

        #1 begin
            // Second cycle: copies should see OLD data (100)
            if (copy1 == 8'd100 && copy2 == 8'd100)
                $display("PASS: Second cycle NBA consistent");
            else
                $display("FAIL: copy1=%d copy2=%d (expected 100,100)", copy1, copy2);
        end

        $finish;
    end

    // Block 1: updates data and copies it
    always @(posedge clk) begin
        copy1 <= data;      // Schedule copy of old data
        data <= data * 2;   // Schedule update of data
    end

    // Block 2: also copies data (potential race with Block 1)
    always @(posedge clk) begin
        copy2 <= data;      // Should get same old value as copy1
    end
endmodule
