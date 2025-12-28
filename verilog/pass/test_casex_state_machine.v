// EXPECT=PASS
// Test casex in state machine (handling unknown states)
module test_casex_state_machine;
    reg clk, reset;
    reg [2:0] state, next_state;
    reg [3:0] output_value;

    localparam IDLE   = 3'b000;
    localparam START  = 3'b001;
    localparam RUN    = 3'b010;
    localparam STOP   = 3'b011;
    localparam ERROR  = 3'b111;

    // State transition using casex to handle X states
    always @(*) begin
        casex (state)
            3'b000:  next_state = START;  // IDLE -> START
            3'b001:  next_state = RUN;    // START -> RUN
            3'b010:  next_state = STOP;   // RUN -> STOP
            3'b011:  next_state = IDLE;   // STOP -> IDLE
            3'bxxx:  next_state = ERROR;  // Unknown -> ERROR
            default: next_state = ERROR;
        endcase
    end

    // Output logic
    always @(*) begin
        casex (state)
            3'b000:  output_value = 4'd0;
            3'b001:  output_value = 4'd1;
            3'b010:  output_value = 4'd2;
            3'b011:  output_value = 4'd3;
            3'bxxx:  output_value = 4'd15;  // Error indicator
            default: output_value = 4'd15;
        endcase
    end

    always @(posedge clk or posedge reset) begin
        if (reset)
            state <= IDLE;
        else
            state <= next_state;
    end

    initial begin
        clk = 0;
        reset = 1;

        #1 reset = 0;
        #1 clk = 1; #1 clk = 0;  // IDLE -> START

        if (state == START && output_value == 4'd1)
            $display("PASS: State machine in START state");
        else
            $display("FAIL: state=%b output=%d", state, output_value);

        #1 clk = 1; #1 clk = 0;  // START -> RUN

        if (state == RUN && output_value == 4'd2)
            $display("PASS: State machine in RUN state");
        else
            $display("FAIL: state=%b output=%d", state, output_value);

        #1 clk = 1; #1 clk = 0;  // RUN -> STOP

        if (state == STOP && output_value == 4'd3)
            $display("PASS: State machine in STOP state");
        else
            $display("FAIL: state=%b output=%d", state, output_value);

        // Test unknown state handling
        state = 3'bxxx;
        #1 if (next_state == ERROR && output_value == 4'd15)
            $display("PASS: casex handles unknown state xxx");
        else
            $display("FAIL: next=%b output=%d", next_state, output_value);

        $finish;
    end
endmodule
