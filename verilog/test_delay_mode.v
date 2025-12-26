// Test: `delay_mode_path, `delay_mode_distributed, `delay_mode_unit
// Feature: Delay mode control directives
// Expected: May pass - directives might be ignored

`delay_mode_path

module test_delay_mode (input a, output y);
  assign #5 y = ~a;
endmodule
