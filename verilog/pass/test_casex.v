// Test casex statement - treats X and Z as don't-care in both case expr and items

module casex_decoder(
  input wire [3:0] opcode,
  output reg [1:0] alu_op,
  output reg mem_write
);

  always @(*) begin
    // casex treats X and ? as don't-care
    casex (opcode)
      4'b000?: begin  // Matches 0000 or 0001 (? is wildcard)
        alu_op = 2'b00;
        mem_write = 1'b0;
      end
      4'b001x: begin  // Matches 0010 or 0011 (x is also wildcard)
        alu_op = 2'b01;
        mem_write = 1'b0;
      end
      4'b01??: begin  // Two wildcards
        alu_op = 2'b10;
        mem_write = 1'b1;
      end
      4'b1???: begin  // Three wildcards
        alu_op = 2'b11;
        mem_write = 1'b0;
      end
      default: begin
        alu_op = 2'b00;
        mem_write = 1'b0;
      end
    endcase
  end

endmodule
