// EXPECT=PASS
// Test hierarchical defparam override
module leaf #(parameter SIZE = 2) (
    input [SIZE-1:0] in,
    output [SIZE-1:0] out
);
    assign out = ~in;
endmodule

module branch (
    input [3:0] data_in,
    output [3:0] data_out
);
    leaf u_leaf (
        .in(data_in),
        .out(data_out)
    );
endmodule

module test_defparam_hierarchical;
    reg [3:0] input_data;
    wire [3:0] output_data;

    branch u_branch (
        .data_in(input_data),
        .data_out(output_data)
    );

    // Override parameter in hierarchical instance
    defparam u_branch.u_leaf.SIZE = 4;

    initial begin
        input_data = 4'b1010;

        #1 begin
            if (output_data == 4'b0101)
                $display("PASS: Hierarchical defparam override worked");
            else
                $display("FAIL: output_data=%b (expected 0101)", output_data);
        end

        $finish;
    end
endmodule
