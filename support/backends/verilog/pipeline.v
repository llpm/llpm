/* LLPM Project library file
 *
 * This file contains implementations of pipeline stuff: registers, LI
 * control, ect..
 */

// This pipeline register contains two buffer slots so it can operate at
// full throughput even with dynamic scheduling
module PipelineReg_DoubleWidth(clk, resetn,
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


// This is buffer slot one. It is always the output
reg [Width-1:0] data1;
reg             valid1;

// This is buffer slot two. It is only filled when data is incoming _and_
// buffer slot one has not been emptied. It will be shifted into buffer slot
// one when that slot has been emptied;
reg [Width-1:0] data2;
reg             valid2;

assign q = data1;
assign q_valid = valid1;

// Backpressure our input if both slots are full
assign d_bp = valid1 && valid2;

// Must we absorb a token?
wire incoming = d_valid && ~d_bp;

// Is the current token leaving us?
wire outgoing = q_valid && ~q_bp;

always@(posedge clk)
begin
    if(~resetn)
    begin
        valid1 <= 1'b0;
        valid2 <= 1'b0;
    end else begin
        if (outgoing && !incoming)
        begin
            valid1 <= valid2;
            data1  <= data2;
        end
        else if (incoming && !outgoing)
        begin
            if (valid1)
            begin
                valid2 <= 1'b1;
                data2  <= d;
            end else begin
                valid1 <= 1'b1;
                data1 <= d;
            end
        end 
        else if (incoming && outgoing)
        begin
            if (valid2)
            begin
                // This condition should not occur!
                $display("We have an incoming value even though #2 is full!");
                // TODO: make this $display work only in simulation
                valid1 <= valid2;
                data1 <= data2;
                valid2 <= 1'b1;
                data2 <= d;
            end else begin
                valid1 <= 1'b1;
                data1 <= d;
            end
        end
    end
end

endmodule;

module PipelineReg_DoubleWidth_NoData(
    clk, resetn,
    d_valid, d_bp,
    q_valid, q_bp);

parameter Width = 8;

input clk;
input resetn;

input             d_valid;
output            d_bp;

output             q_valid;
input              q_bp;


// This is buffer slot one. It is always the output
reg             valid1;

// This is buffer slot two. It is only filled when data is incoming _and_
// buffer slot one has not been emptied. It will be shifted into buffer slot
// one when that slot has been emptied;
reg             valid2;

assign q_valid = valid1;

// Backpressure our input if both slots are full
assign d_bp = valid1 && valid2;

// Must we absorb a token?
wire incoming = d_valid && ~d_bp;

// Is the current token leaving us?
wire outgoing = q_valid && ~q_bp;

always@(posedge clk)
begin
    if(~resetn)
    begin
        valid1 <= 1'b0;
        valid2 <= 1'b0;
    end else begin
        if (outgoing && !incoming)
        begin
            valid1 <= valid2;
        end
        else if (incoming && !outgoing)
        begin
            if (valid1)
            begin
                valid2 <= 1'b1;
            end else begin
                valid1 <= 1'b1;
            end
        end 
        else if (incoming && outgoing)
        begin
            if (valid2)
            begin
                // This condition should not occur!
                $display("We have an incoming value even though #2 is full!");
                // TODO: make this $display work only in simulation
                valid1 <= valid2;
                valid2 <= 1'b1;
            end else begin
                valid1 <= 1'b1;
            end
        end
    end
end

endmodule;

// This pipeline register contains only one buffer slot so it can only
// operate at 1/2 throughput unless it is optimized via static scheduling
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

