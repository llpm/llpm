/* LLPM Project library file
 *
 * This file contains implementations of pipeline stuff: registers, LI
 * control, ect..
 */

module PipelineReg_SingleStore(clk, resetn,
    d, d_valid, d_bp,
    q, q_valid, q_bp);

parameter Width = 8;

input clk;
input resetn;

input [Width-1:0] d;
input             d_valid;
output            d_bp;

output [Width-1:0] q;
output             q_valid;
input              q_bp;


reg [Width-1:0] data;
reg             valid;

assign q = data;
assign q_valid = valid;

// Backpressure our input if we have data
assign d_bp = valid;

// Must we absorb a token?
wire incoming = d_valid && ~d_bp;

// Is the current token leaving us?
wire outgoing = valid && ~q_bp;

always@(posedge clk)
begin
    if(~resetn)
    begin
        valid <= 1'b0;
    end else begin
        if (outgoing)
        begin
            valid <= 1'b0;
        end
        else if (incoming)
        begin
            valid <= 1'b1;
            data  <= d;
        end 
    end
end

endmodule;

