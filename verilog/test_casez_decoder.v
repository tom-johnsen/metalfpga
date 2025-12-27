// EXPECT=PASS
// Test casez for instruction decoding (practical use case)
module test_casez_decoder;
    reg [7:0] opcode;
    reg [3:0] instruction_type;

    // Typical instruction decoder using casez
    always @(*) begin
        casez (opcode)
            8'b0000_????: instruction_type = 4'd0;  // ALU operations
            8'b0001_????: instruction_type = 4'd1;  // Load/Store
            8'b0010_00??: instruction_type = 4'd2;  // Branch
            8'b0010_01??: instruction_type = 4'd3;  // Jump
            8'b0011_????: instruction_type = 4'd4;  // System
            8'b1???_????: instruction_type = 4'd15; // Extended instruction set
            default:      instruction_type = 4'd14; // Invalid
        endcase
    end

    initial begin
        opcode = 8'b0000_0101;
        #1 if (instruction_type == 4'd0)
            $display("PASS: Decoded ALU instruction (0000_0101)");
        else
            $display("FAIL: type=%d (expected 0)", instruction_type);

        opcode = 8'b0001_1111;
        #1 if (instruction_type == 4'd1)
            $display("PASS: Decoded Load/Store (0001_1111)");
        else
            $display("FAIL: type=%d (expected 1)", instruction_type);

        opcode = 8'b0010_0010;
        #1 if (instruction_type == 4'd2)
            $display("PASS: Decoded Branch (0010_0010)");
        else
            $display("FAIL: type=%d (expected 2)", instruction_type);

        opcode = 8'b0010_0101;
        #1 if (instruction_type == 4'd3)
            $display("PASS: Decoded Jump (0010_0101)");
        else
            $display("FAIL: type=%d (expected 3)", instruction_type);

        opcode = 8'b1010_1010;
        #1 if (instruction_type == 4'd15)
            $display("PASS: Decoded Extended (1010_1010)");
        else
            $display("FAIL: type=%d (expected 15)", instruction_type);

        opcode = 8'b0100_0000;
        #1 if (instruction_type == 4'd14)
            $display("PASS: Invalid opcode goes to default");
        else
            $display("FAIL: type=%d (expected 14)", instruction_type);

        $finish;
    end
endmodule
