// EXPECT=PASS
// Test: casez vs casex with X on wide buses
// Feature: X should block casez match, casex should treat X as don't care

module test_wide_casez_xz;
  reg [127:0] value;
  reg hit_casez;
  reg hit_casex;

  initial begin
    value = 128'h0000000000000000000000000000000x;

    hit_casez = 1'b0;
    hit_casex = 1'b0;

    casez (value)
      128'h00000000000000000000000000000000: hit_casez = 1'b1;
      default: hit_casez = 1'b0;
    endcase

    casex (value)
      128'h00000000000000000000000000000000: hit_casex = 1'b1;
      default: hit_casex = 1'b0;
    endcase

    if (hit_casez != 1'b0) begin
      $display("FAIL: casez matched X");
    end else begin
      $display("PASS: casez did not match X");
    end

    if (hit_casex != 1'b1) begin
      $display("FAIL: casex did not match X/Z");
    end else begin
      $display("PASS: casex matched X/Z");
    end

    $finish;
  end
endmodule
