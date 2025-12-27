// EXPECT=PASS
// Test combinational logic evaluation order
module test_combinational_race;
    reg [7:0] a, b, c;
    wire [7:0] x, y, z;

    // These continuous assignments can execute in any order
    // but result should be deterministic
    assign x = a + b;
    assign y = x + c;    // Depends on x
    assign z = x + y;    // Depends on both x and y

    initial begin
        a = 8'd10;
        b = 8'd20;
        c = 8'd30;

        #1 begin
            // x = 10 + 20 = 30
            // y = 30 + 30 = 60
            // z = 30 + 60 = 90
            if (x == 8'd30)
                $display("PASS: x = %d", x);
            else
                $display("FAIL: x=%d (expected 30)", x);

            if (y == 8'd60)
                $display("PASS: y = %d", y);
            else
                $display("FAIL: y=%d (expected 60)", y);

            if (z == 8'd90)
                $display("PASS: z = %d", z);
            else
                $display("FAIL: z=%d (expected 90)", z);
        end

        // Update and check again
        #1 begin
            a = 8'd5;
            b = 8'd15;
        end

        #1 begin
            // x = 5 + 15 = 20
            // y = 20 + 30 = 50
            // z = 20 + 50 = 70
            if (x == 8'd20 && y == 8'd50 && z == 8'd70)
                $display("PASS: Combinational update propagated correctly");
            else
                $display("FAIL: x=%d y=%d z=%d (expected 20,50,70)", x, y, z);
        end

        $finish;
    end
endmodule
