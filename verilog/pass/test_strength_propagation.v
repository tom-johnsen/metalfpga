// EXPECT=PASS
// Test drive strength propagation through gates
module test_strength_propagation;
    reg input_val;
    wire (strong1, strong0) strong_out;
    wire (pull1, pull0) pull_out;
    wire (weak1, weak0) weak_out;
    wire resolved;

    // Strong driver
    buf (strong1, strong0) strong_buf (strong_out, input_val);

    // Pull driver
    buf (pull1, pull0) pull_buf (pull_out, input_val);

    // Weak driver
    buf (weak1, weak0) weak_buf (weak_out, input_val);

    // Resolution: strong > pull > weak
    // If strong_out=1, pull_out=0, weak_out=0, resolved should be 1
    assign resolved = strong_out;  // Strong wins

    initial begin
        input_val = 1'b0;

        #1 begin
            if (strong_out === 1'b0)
                $display("PASS: Strong driver outputs 0");
            else
                $display("FAIL: strong_out=%b", strong_out);

            if (pull_out === 1'b0)
                $display("PASS: Pull driver outputs 0");
            else
                $display("FAIL: pull_out=%b", pull_out);
        end

        #1 input_val = 1'b1;

        #1 begin
            if (strong_out === 1'b1)
                $display("PASS: Strong driver outputs 1");
            else
                $display("FAIL: strong_out=%b", strong_out);

            if (weak_out === 1'b1)
                $display("PASS: Weak driver outputs 1");
            else
                $display("FAIL: weak_out=%b", weak_out);
        end

        $finish;
    end
endmodule
