/* LLPM Project library file
 *
 * This file contains memory implementations. Registers, block rams, etc..
 */

module RTLReg(clk, resetn, write_req, write_req_valid, write_req_bp, write_resp_valid, write_resp_bp, read, read_valid, read_bp);

parameter Width = 8;

input clk;
input resetn;

input [Width-1:0] write_req;
input             write_req_valid;
output            write_req_bp;

output write_resp_valid;
input  write_resp_bp;

output [Width-1:0] read;
output             read_valid;
input              read_bp;


reg             valid;
reg [Width-1:0] data;

assign read = data;
assign read_valid = valid;

assign write_req_bp = write_resp_bp;
assign write_resp_valid = write_req_valid;

always@(posedge clk)
begin
    if(~resetn)
    begin
        valid <= 1'b0;
    end else begin
        if (write_req_valid && ~write_req_bp)
        begin
            valid <= 1'b1;
            data <= write_req;
        end
    end
end

endmodule;
