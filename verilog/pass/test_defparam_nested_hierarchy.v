// EXPECT=PASS
// Test defparam resolution through multiple hierarchy levels
module leaf #(parameter SIZE = 4) (
    input [SIZE-1:0] in,
    output [SIZE-1:0] out
);
    assign out = ~in;
endmodule

module branch #(parameter WIDTH = 4) (
    input [WIDTH-1:0] data_in,
    output [WIDTH-1:0] data_out
);
    leaf #(.SIZE(WIDTH)) u_leaf (
        .in(data_in),
        .out(data_out)
    );
endmodule

module trunk (
    input [7:0] root_in,
    output [7:0] root_out
);
    branch u_branch (
        .data_in(root_in),
        .data_out(root_out)
    );
endmodule

module test_defparam_nested_hierarchy;
    reg [7:0] input_data;
    wire [7:0] output_data;

    trunk u_trunk (
        .root_in(input_data),
        .root_out(output_data)
    );

    // Override parameter through 3 levels of hierarchy
    defparam u_trunk.u_branch.u_leaf.SIZE = 8;

    initial begin
        input_data = 8'b10101010;

        #1 begin
            if (output_data == 8'b01010101)
                $display("PASS: Defparam through 3 hierarchy levels");
            else
                $display("FAIL: output_data=%b (expected 01010101)", output_data);
        end

        $finish;
    end
endmodule
