// Test: SystemVerilog enumerated types
// Feature: enum declarations
// Expected: Should fail - SystemVerilog construct

module test_enum;
  typedef enum {IDLE, ACTIVE, DONE} state_t;
  state_t state, next_state;

  always @* begin
    case (state)
      IDLE: next_state = ACTIVE;
      ACTIVE: next_state = DONE;
      DONE: next_state = IDLE;
    endcase
  end
endmodule
