/* LLPM Project library file
 *
 * This file contains verilog implementations of Forks
 */

`default_nettype none

// An actual fork which contains duplicate valid bits for each output
module Fork(clk, resetn,
            din, din_valid, din_bp,
            dout, dout_valid, dout_bp);

parameter Width = 8;
parameter NumOutputs = 4;

input wire clk;
input wire resetn;

input wire [Width-1:0]  din;
input wire             din_valid;
output wire            din_bp;

output wire [Width-1:0]      dout;
output wire [NumOutputs-1:0] dout_valid;
input  wire [NumOutputs-1:0] dout_bp;

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

endmodule

// Same thing without data
module Fork_VoidData(
    clk, resetn,
    din_valid, din_bp,
    dout_valid, dout_bp);

parameter Width = 8;
parameter NumOutputs = 4;

input wire clk;
input wire resetn;

input  wire             din_valid;
output wire             din_bp;

output wire [NumOutputs-1:0] dout_valid;
input  wire [NumOutputs-1:0] dout_bp;

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

endmodule

// A "virtual" fork which has no actual storage and merely computes
// the AND of backpressure
module VirtFork(clk, resetn,
                din, din_valid, din_bp,
                dout, dout_valid, dout_bp);

parameter Width = 8;
parameter NumOutputs = 4;

input wire clk;
input wire resetn;

input wire [Width-1:0]  din;
input wire              din_valid;
output wire             din_bp;

output wire [Width-1:0]      dout;
output wire [NumOutputs-1:0] dout_valid;
input  wire [NumOutputs-1:0] dout_bp;

wire [NumOutputs-1:0] accepted = (~dout_bp & dout_valid);

assign dout = din;
assign din_bp = !(&accepted);
assign dout_valid = {NumOutputs{din_valid}};

endmodule

// Same thing, without data
module VirtFork_VoidData(
    clk, resetn,
    din_valid, din_bp,
    dout_valid, dout_bp);

parameter Width = 8;
parameter NumOutputs = 4;

input wire clk;
input wire resetn;

input  wire             din_valid;
output wire             din_bp;

output wire [NumOutputs-1:0] dout_valid;
input  wire [NumOutputs-1:0] dout_bp;

wire [NumOutputs-1:0] accepted = (~dout_bp & dout_valid);

assign din_bp = !(&accepted);
assign dout_valid = {NumOutputs{din_valid}};

endmodule

`default_nettype wire
