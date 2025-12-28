// EXPECT=PASS
// Test: Verilog attributes (* key = value *)
// Feature: Module, net, and statement attributes

(* top_module *)
module test_attribute;
  (* keep = 1 *)
  wire w1;

  (* ram_style = "block" *)
  reg [7:0] memory [0:255];

  (* full_case, parallel_case *)
  always @* begin
    case (w1)
      1'b0: (* synthesis, keep *) w1 = 1'b1;
      1'b1: w1 = 1'b0;
    endcase
  end
endmodule
