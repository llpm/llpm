/* LLPM Project library file
 *
 * This file contains verilog implementations of Forks
 */


// An actual fork which contains duplicate valid bits for each output
module Fork(clk, resetn,
            din, din_valid, din_bp,
            dout, dout_valid, dout_bp);

parameter Width = 8;
parameter NumOutputs = 4;

input clk;
input resetn;

input [Width-1:0]  din;
input              din_valid;
output             din_bp;

output [Width-1:0]      dout;
output [NumOutputs-1:0] dout_valid;
input  [NumOutputs-1:0] dout_bp;

assign dout = din;

reg [NumOutputs-1:0] previouslyAccepted;
wire [NumOutputs-1:0] accepted =
    (~dout_bp & dout_valid) | previouslyAccepted;

assign din_bp = !(&accepted);
assign dout_valid = {NumOutputs{din_valid}} & (~previouslyAccepted);

always@(posedge clk)
begin
    if (~resetn)
    begin
        previouslyAccepted <= {NumOutputs{1'b0}};
    end else begin
        if (din_valid && ~din_bp)
        begin
            previouslyAccepted <= {NumOutputs{1'b0}};
        end else begin
            previouslyAccepted <= accepted;
        end
    end
end

endmodule;

