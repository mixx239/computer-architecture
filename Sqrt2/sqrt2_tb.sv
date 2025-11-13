`include "sqrt2.sv"

module sqrt2_tb;
	reg CLK;
	always #1 CLK = ~CLK;
	wire IS_NAN;
	wire IS_PINF;
	wire IS_NINF;
	wire RESULT;
	wire [15:0] IO_DATA;
	reg ENABLE;
	reg [15:0] test_data;  

	assign IO_DATA = test_data;

	sqrt2 wires (
	.IO_DATA(IO_DATA),
	.IS_NAN(IS_NAN),
	.IS_PINF(IS_PINF),
	.IS_NINF(IS_NINF),
	.RESULT(RESULT),
	.CLK(CLK),
	.ENABLE(ENABLE)
	);

	initial begin
		CLK = 0;
		ENABLE = 0;

		test_data = 16'h3C00;
		#1 ENABLE = 1;
		#1 test_data = 16'bzzzzzzzzzzzzzzzz;
		wait(RESULT);
		if (IO_DATA !== 16'h3C00) $display("Test 1 FAILED: got %h", IO_DATA);
		else $display("Test 1 PASSED - sqrt(3C00) = 3C00");
		#1 ENABLE = 0;

		test_data = 16'h4400;
		#1 ENABLE = 1;
		#1 test_data = 16'bzzzzzzzzzzzzzzzz;
		wait(RESULT);
		if (IO_DATA !== 16'h4000) $display("Test 2 FAILED");
		else $display("Test 2 PASSED - sqrt(4400) = 4000");
		#1 ENABLE = 0;
		
		test_data = 16'h3400;
		#1 ENABLE = 1;
		#1 test_data = 16'bzzzzzzzzzzzzzzzz;
		wait(RESULT);
		if (IO_DATA !== 16'h3800) $display("Test 3 FAILED");
		else $display("Test 3 PASSED - sqrt(3400) = 3800");
		#1 ENABLE = 0;

		test_data = 16'h2392;
		#1 ENABLE = 1;
		#1 test_data = 16'bzzzzzzzzzzzzzzzz;
		wait(RESULT);
		if (IO_DATA !== 16'h2FC8) $display("Test 4 FAILED");
		else $display("Test 4 PASSED - sqrt(2392) = 2FC8");
		#1 ENABLE = 0;
		$finish;
	end
endmodule