/* LLPM Project library file
 *
 * This file contains a number of different implementations of the LLPM
 * 'select' vertex with different properties. Their x->a and LI control signal
 * paths are purely combinatorial. Clock and reset signals are provided only
 * so that implementations may enforce fairness.
 */
`default_nettype none

module LLPM_IdxSelect(clk, resetn,
    idx, idx_valid, idx_bp,
    x, x_valid, x_bp,
    a, a_valid, a_bp);

parameter Width = 8;
parameter NumInputs = 4;
parameter CLog2NumInputs = 2;

input wire clk;
input wire resetn;

input wire      idx [CLog2NumInputs-1:0];
input wire      idx_valid;
output wire     idx_bp;

input wire      [Width-1:0] x       [NumInputs-1:0];
input wire                  x_valid [NumInputs-1:0];
output reg             x_bp    [NumInputs-1:0];

output wire [Width-1:0] a;
output wire             a_valid;
input  wire             a_bp;

assign valid  = x_valid[idx] && idx_valid;

assign a = x[idx];
assign a_valid = valid;

assign idx_bp = a_bp || !x_valid[idx];

integer i;
always@(*)
begin
    for (i=0; i<NumInputs; i = i + 1)
    begin
        x_bp[i] = (idx != i) ||
                  a_bp || 
                  !idx_valid;
    end
end

endmodule

module LLPM_IdxSelectNoData(clk, resetn,
    idx, idx_valid, idx_bp,
    x_valid, x_bp,
    a_valid, a_bp);

parameter Width = 8;
parameter NumInputs = 4;
parameter CLog2NumInputs = 2;

input wire clk;
input wire resetn;

input wire      idx [CLog2NumInputs-1:0];
input wire      idx_valid;
output wire     idx_bp;

input wire                  x_valid [NumInputs-1:0];
output reg             x_bp    [NumInputs-1:0];

output wire             a_valid;
input  wire             a_bp;

reg valid;
assign valid  = x_valid[idx] && idx_valid;

assign a_valid = valid;

assign idx_bp = a_bp || !x_valid[idx];

integer i;
always@(*)
begin
    for (i=0; i<NumInputs; i = i + 1)
    begin
        x_bp[i] = (idx != i) ||
                  a_bp || 
                  !idx_valid;
    end
end

endmodule

`default_nettype wire
