// module sqrt2(
//     inout   wire[15:0] IO_DATA,
//     output  wire IS_NAN,
//     output  wire IS_PINF,
//     output  wire IS_NINF,
//     output  wire RESULT,
//     input   wire CLK,
//     input   wire ENABLE
// ); 
    
// reg [15:0] ans;
// reg [15:0] first_data;
// reg [4:0] iter_count;
// reg [4:0] exp;
// reg [9:0] mant;
// reg [10:0] res_mant;
// reg [21:0] mant22;
// reg [21:0] tmp_mant22;
// reg [4:0] number_of_shifts;

// reg result;
// reg calculating;
// reg sign;
// reg is_zero;
// reg is_nan;
// reg is_pinf;
// reg is_ninf;
// reg is_denorm;

// assign IO_DATA = (ENABLE && (calculating || result)) ? ans : 16'bzzzzzzzzzzzzzzzz;
// assign IS_NAN = is_nan;
// assign IS_PINF = is_pinf;
// assign IS_NINF = is_ninf;
// assign RESULT = result;

// always @(CLK) begin
//     if (!ENABLE) begin
//         calculating <= 0;
//         result <= 0;
//         is_nan <= 0;
//         is_pinf <= 0;
//         is_ninf <= 0;
//         is_denorm <= 0;
//         iter_count <= 0;
//         number_of_shifts <= 0;
//     end else if (!result) begin
//         if (!calculating) begin
//             first_data = IO_DATA;
//             sign = first_data[15];
//             exp = first_data[14:10];
//             mant = first_data[9:0];
//             mant22 = 0;
//             res_mant = 0;
//             tmp_mant22 = 0;
//             is_zero = (exp == 0) && (mant == 0);
//             is_nan = (sign && !is_zero) || ((exp == 5'b11111) && (mant != 0));
//             is_pinf = (!sign) && (exp == 5'b11111) && (mant == 0);
//             is_denorm = (exp == 0) && (mant != 0);
//             calculating = 1;
//             result = 0;
//             iter_count = 0;

//             if ((exp == 5'b11111) && (mant != 0)) begin
//                 first_data[9] = 1;
//                 ans <= first_data;
//                 result <= 1;
//             end else if(sign && !is_zero) begin
//                 ans <= 16'hFE00;
//                 result <= 1;
//             end else if(is_pinf) begin
//                 ans <= first_data;
//                 result <= 1;
//                 is_pinf <= !sign;
//             end else if(is_zero) begin
//                 ans <= first_data;
//                 result <= 1;
//             end else begin
//                 if (!is_denorm) begin
//                     if (exp[0]) begin 
//                         exp <= ((exp >> 1) + 8);
//                         mant22 = mant;
//                         mant22 = mant22 << 10;
//                         mant22[20] = 1;
//                     end else begin
//                         exp <= ((exp >> 1) + 7);
//                         mant22 = mant;
//                         mant22 = mant22 << 11;
//                         mant22[21] = 1;
//                     end 
//                 end else begin
//                     while (!mant[9]) begin
//                         mant = mant << 1;
//                         number_of_shifts = number_of_shifts + 1;
//                     end 
//                     mant22 = mant;
//                     mant22 = mant22 << 11;
//                     number_of_shifts = number_of_shifts + 1;
//                     if (number_of_shifts[0]) begin
//                         mant22 = mant22 << 1;
//                         exp <= (7 - (number_of_shifts >> 1));
//                     end else begin
//                         exp <= (8 - (number_of_shifts >> 1));
//                     end 
//                 end  
//             end
//         end else begin
//             if (!result && (iter_count < 11)) begin
//                 res_mant = res_mant << 1;
//                 tmp_mant22 = (res_mant << 1) + 1;
//                 tmp_mant22 = (tmp_mant22 << ((10 - iter_count) << 1));
//                 if (mant22 >= tmp_mant22) begin
//                     mant22 = mant22 - tmp_mant22;
//                     res_mant = res_mant + 1;
//                 end
//                 iter_count <= iter_count + 1;
//                 ans <= {1'b0, exp, res_mant[9:0]};
//             end else begin
//                 calculating <= 0;
//                 result <= 1;
//             end 
//         end 
//     end 
// end
// endmodule



// // module lol(input wire a, input wire b, output wire c);
// // reg t = a or b;
// // assign c = t;


// // endmodule

// // module tb;
// // wire a;
// // wire b;
// // wire c;
// // lol wires(
// //     .a(a),
// //     .b(b),
// //     .c(c)
// // )

// // endmodule






module jktrig(input wire a, input wire b, output wire q, output wire nq);
    reg x;

    always @* begin
    if (a == 0 && b == 1) begin
        x <= 0;
    end

    if (a == 1 && b == 0) begin
        x <= 1;
    end

if (a == 1 && b == 1) begin
        x <= !x;
    end




    end

        assign q = x;
    assign nq = ~q;


endmodule





module tb;

reg x;
reg y;
wire z;
wire nz;

jktrig jk(.a(x), .b(y), .q(z), .nq(nz));



initial begin
    x = 1;
    y = 0;
    #10;
    

$display("aaaaaaaaaaaaa");
if(z == 1) begin
    $display("1");
end
if(z == 0) begin
    $display("0");
end
end 


endmodule