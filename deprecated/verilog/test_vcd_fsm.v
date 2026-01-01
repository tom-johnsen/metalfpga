// EXPECT=PASS
// Finite state machine - tests state transitions in VCD
module test_vcd_fsm;
  reg clk;
  reg reset;
  reg [2:0] state;
  reg [7:0] counter;

  // State encoding
  localparam IDLE  = 3'd0;
  localparam LOAD  = 3'd1;
  localparam EXEC  = 3'd2;
  localparam STORE = 3'd3;
  localparam DONE  = 3'd4;

  initial begin
    $dumpfile("fsm.vcd");
    $dumpvars(0, test_vcd_fsm);

    clk = 0;
    reset = 1;
    state = IDLE;
    counter = 0;

    #2 reset = 0;
    #18 $finish;
  end

  always #1 clk = ~clk;

  always @(posedge clk) begin
    if (reset) begin
      state <= IDLE;
      counter <= 0;
    end else begin
      case (state)
        IDLE:  state <= LOAD;
        LOAD:  state <= EXEC;
        EXEC:  begin
          state <= (counter == 8'd3) ? STORE : EXEC;
          counter <= counter + 1;
        end
        STORE: state <= DONE;
        DONE:  state <= IDLE;
        default: state <= IDLE;
      endcase
    end
  end
endmodule
