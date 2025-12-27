// EXPECT=PASS
// Test casez with wide patterns (realistic bit widths)
module test_casez_wide_patterns;
    reg [15:0] address;
    reg [2:0] region;

    always @(*) begin
        casez (address)
            16'h0???: region = 3'd0;  // 0x0000-0x0FFF: Boot ROM
            16'h1???: region = 3'd1;  // 0x1000-0x1FFF: RAM
            16'h2???: region = 3'd2;  // 0x2000-0x2FFF: Peripherals
            16'h3???: region = 3'd3;  // 0x3000-0x3FFF: External
            16'hf???: region = 3'd7;  // 0xF000-0xFFFF: Flash
            default:  region = 3'd4;  // Unmapped
        endcase
    end

    initial begin
        address = 16'h0123;
        #1 if (region == 3'd0)
            $display("PASS: casez wide pattern 0x0123 -> Boot ROM");
        else
            $display("FAIL: region=%d (expected 0)", region);

        address = 16'h1FFF;
        #1 if (region == 3'd1)
            $display("PASS: casez wide pattern 0x1FFF -> RAM");
        else
            $display("FAIL: region=%d (expected 1)", region);

        address = 16'h2500;
        #1 if (region == 3'd2)
            $display("PASS: casez wide pattern 0x2500 -> Peripherals");
        else
            $display("FAIL: region=%d (expected 2)", region);

        address = 16'hF800;
        #1 if (region == 3'd7)
            $display("PASS: casez wide pattern 0xF800 -> Flash");
        else
            $display("FAIL: region=%d (expected 7)", region);

        address = 16'h5000;
        #1 if (region == 3'd4)
            $display("PASS: casez unmapped address -> default");
        else
            $display("FAIL: region=%d (expected 4)", region);

        $finish;
    end
endmodule
