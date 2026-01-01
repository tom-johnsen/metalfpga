// EXPECT=PASS
// Test generate with parameter calculations
module test_generate_recursive_param;
    parameter N = 4;
    localparam N_SQUARED = N * N;
    localparam N_CUBED = N * N * N;

    wire [N_SQUARED-1:0] result_sq;
    wire [N_CUBED-1:0] result_cube;
    reg data;

    genvar i;

    // Generate N*N instances
    generate
        for (i = 0; i < N_SQUARED; i = i + 1) begin : square_gen
            buf (result_sq[i], data);
        end
    endgenerate

    // Generate N*N*N instances
    generate
        for (i = 0; i < N_CUBED; i = i + 1) begin : cube_gen
            buf (result_cube[i], data);
        end
    endgenerate

    initial begin
        data = 1'b1;

        #1 begin
            // N=4, N_SQUARED=16, all bits should be 1
            if (result_sq == 16'hFFFF)
                $display("PASS: Generated N^2=16 instances");
            else
                $display("FAIL: result_sq=%h", result_sq);

            // N_CUBED=64, all bits should be 1
            if (result_cube == 64'hFFFFFFFFFFFFFFFF)
                $display("PASS: Generated N^3=64 instances");
            else
                $display("FAIL: result_cube=%h", result_cube);
        end

        $finish;
    end
endmodule
