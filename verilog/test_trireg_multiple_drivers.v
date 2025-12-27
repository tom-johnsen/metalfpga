// EXPECT=PASS
// Test trireg with multiple drivers sequentially
module test_trireg_multiple_drivers;
    reg drv_a, drv_b, drv_c;
    reg en_a, en_b, en_c;
    trireg storage;

    bufif1 (storage, drv_a, en_a);
    bufif1 (storage, drv_b, en_b);
    bufif1 (storage, drv_c, en_c);

    initial begin
        drv_a = 1'b0;
        drv_b = 1'b0;
        drv_c = 1'b0;
        en_a = 1'b0;
        en_b = 1'b0;
        en_c = 1'b0;

        // Driver A sets to 1
        #1 begin
            drv_a = 1'b1;
            en_a = 1'b1;
        end

        #1 begin
            if (storage === 1'b1)
                $display("PASS: Driver A set trireg to 1");
            else
                $display("FAIL: storage=%b (expected 1)", storage);
        end

        // Disable A, enable B with 0
        #1 begin
            en_a = 1'b0;
            drv_b = 1'b0;
            en_b = 1'b1;
        end

        #1 begin
            if (storage === 1'b0)
                $display("PASS: Driver B set trireg to 0");
            else
                $display("FAIL: storage=%b (expected 0)", storage);
        end

        // Disable B, trireg should hold
        #1 en_b = 1'b0;

        #1 begin
            if (storage === 1'b0)
                $display("PASS: Trireg holds 0 after B disconnect");
            else
                $display("FAIL: storage=%b (expected 0)", storage);
        end

        // Driver C sets to 1
        #1 begin
            drv_c = 1'b1;
            en_c = 1'b1;
        end

        #1 begin
            if (storage === 1'b1)
                $display("PASS: Driver C set trireg to 1");
            else
                $display("FAIL: storage=%b (expected 1)", storage);
        end

        // All disabled
        #1 begin
            en_a = 1'b0;
            en_b = 1'b0;
            en_c = 1'b0;
        end

        #2 begin
            if (storage === 1'b1 || storage === 1'bx)
                $display("PASS: Trireg maintains or decays gracefully");
            else
                $display("FAIL: storage=%b", storage);
        end

        $finish;
    end
endmodule
