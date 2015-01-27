/* LLPM Project library file
 *
 * This file contains a number of different implementations of the LLPM
 * 'select' vertex with different properties. Their x->a and LI control signal
 * paths are purely combinatorial. Clock and reset signals are provided only
 * so that implementations may enforce fairness.
 */
`default_nettype none

// This select implementation uses an arbiter which always favors the highest
// number input. Can cause starvation on other inputs. Cheap and if
// necessary, flow control must be implemented elsewhere
module LLPM_Select_Priority(clk, resetn, x, x_valid, x_bp, a, a_valid, a_bp);

parameter Width = 8;
parameter NumInputs = 4;
parameter CLog2NumInputs = 2;

input wire clk;
input wire resetn;

input wire      [Width-1:0] x       [NumInputs-1:0];
input wire                  x_valid [NumInputs-1:0];
output reg             x_bp    [NumInputs-1:0];

output wire [Width-1:0] a;
output wire             a_valid;
input  wire             a_bp;

reg has_valid;
reg [CLog2NumInputs-1:0] select;

integer i;
always@(*)
begin
    select = {CLog2NumInputs{1'bx}};
    has_valid = 1'b0;
    for (i=0; i<NumInputs; i = i + 1)
    begin
        if (x_valid[i])
        begin
            select = i[CLog2NumInputs-1:0];
            has_valid = 1'b1;
        end
    end
end

integer j;
always@(*)
begin
    for (j=0; j<NumInputs; j = j + 1)
    begin
        if (a_bp)
            x_bp[j] = 1'b1;
        else
            x_bp[j] = ~(select == j[CLog2NumInputs-1:0]);
    end
end

assign a_valid = has_valid;
assign a       = x[select];

endmodule

module LLPM_Select_PriorityNoData(
    clk, resetn,
    x_valid, x_bp,
    a_valid, a_bp);

parameter Width = 8;
parameter NumInputs = 4;
parameter CLog2NumInputs = 2;

input wire clk;
input wire resetn;

input wire              x_valid [NumInputs-1:0];
output reg         x_bp    [NumInputs-1:0];

output wire             a_valid;
input  wire             a_bp;

reg has_valid;
reg [CLog2NumInputs-1:0] select;

integer i;
always@(*)
begin
    select = {CLog2NumInputs{1'bx}};
    has_valid = 1'b0;
    for (i=0; i<NumInputs; i = i + 1)
    begin
        if (x_valid[i])
        begin
            select = i[CLog2NumInputs-1:0];
            has_valid = 1'b1;
        end
    end
end

integer j;
always@(*)
begin
    for (j=0; j<NumInputs; j = j + 1)
    begin
        if (a_bp)
            x_bp[j] = 1'b1;
        else
            x_bp[j] = ~(select == j[CLog2NumInputs-1:0]);
    end
end

assign a_valid = has_valid;

endmodule
`default_nettype wire
