// EXPECT=PASS
// Test trireg interaction with different drive strengths
module test_trireg_strength_interaction;
    reg weak_driver, strong_driver;
    reg weak_en, strong_en;
    trireg net;

    // Weak and strong drivers
    bufif1 (weak1, weak0) (net, weak_driver, weak_en);
    bufif1 (strong1, strong0) (net, strong_driver, strong_en);

    initial begin
        weak_driver = 1'b0;
        strong_driver = 1'b0;
        weak_en = 1'b0;
        strong_en = 1'b0;

        // Weak driver charges to 1
        #1 begin
            weak_driver = 1'b1;
            weak_en = 1'b1;
        end

        #1 begin
            if (net === 1'b1)
                $display("PASS: Weak driver charged trireg to 1");
            else
                $display("FAIL: net=%b (expected 1)", net);
        end

        // Strong driver takes over with 0
        #1 begin
            strong_driver = 1'b0;
            strong_en = 1'b1;
        end

        #1 begin
            if (net === 1'b0)
                $display("PASS: Strong driver overrides weak (0 wins)");
            else
                $display("FAIL: net=%b (expected 0)", net);
        end

        // Disconnect both, trireg should hold last value
        #1 begin
            weak_en = 1'b0;
            strong_en = 1'b0;
        end

        #1 begin
            if (net === 1'b0)
                $display("PASS: Trireg holds value after strong disconnect");
            else
                $display("FAIL: net=%b (expected 0)", net);
        end

        $finish;
    end
endmodule
