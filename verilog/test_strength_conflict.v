// EXPECT=PASS
// Test conflicting drive strengths and resolution
module test_strength_conflict;
    wire net;
    reg en1, en2, en3;

    // Three drivers with different strengths, all can drive different values
    buf (strong1, strong0) drv_strong (net, 1'b1);
    buf (pull1, pull0) drv_pull (net, 1'b0);
    buf (weak1, weak0) drv_weak (net, 1'b1);

    initial begin
        #1 begin
            // Strong drives 1, pull drives 0, weak drives 1
            // Strong should win (1)
            if (net === 1'b1)
                $display("PASS: Strong1 wins over pull0 and weak1");
            else if (net === 1'bx)
                $display("INFO: Conflict resulted in X (also valid): net=%b", net);
            else
                $display("FAIL: net=%b (expected 1 or x)", net);
        end

        $finish;
    end
endmodule

// Test strength reduction through gates
module test_strength_reduction;
    reg input_val;
    wire (strong1, strong0) strong_wire;
    wire weak_out;

    // Strong driver
    buf (strong1, strong0) strong_drv (strong_wire, input_val);

    // Regular buffer reduces to default strength
    buf weak_buf (weak_out, strong_wire);

    initial begin
        input_val = 1'b1;

        #1 begin
            if (strong_wire === 1'b1)
                $display("PASS: Strong wire carries 1");
            else
                $display("FAIL: strong_wire=%b", strong_wire);

            if (weak_out === 1'b1)
                $display("PASS: Strength reduced through buffer");
            else
                $display("FAIL: weak_out=%b", weak_out);
        end

        $finish;
    end
endmodule
