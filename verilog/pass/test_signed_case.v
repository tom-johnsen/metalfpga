// Test signed values in case statements
module test_signed_case(
    input signed [7:0] val,
    output reg [2:0] range_category,
    output reg is_special
);
    always @(*) begin
        // Categorize signed value by range
        if (val > 8'sh40) begin
            range_category = 3'b100;  // Large positive
        end else if (val > 8'sh00) begin
            range_category = 3'b010;  // Small positive
        end else if (val == 8'sh00) begin
            range_category = 3'b001;  // Zero
        end else if (val > 8'shC0) begin
            range_category = 3'b011;  // Small negative
        end else begin
            range_category = 3'b101;  // Large negative
        end

        // Check for special values
        case (val)
            8'sh00: is_special = 1'b1;  // Zero
            8'sh7F: is_special = 1'b1;  // Max positive
            8'sh80: is_special = 1'b1;  // Max negative
            8'shFF: is_special = 1'b1;  // -1
            default: is_special = 1'b0;
        endcase
    end
endmodule
