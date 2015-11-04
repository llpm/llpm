/* LLPM Project library file
 *
 * This file contains memory implementations. Registers, block rams, etc..
 */
`default_nettype none

module RTLReg(clk, resetn,
    write_req, write_req_valid, write_req_bp,
    write_resp_valid, write_resp_bp,
    read, read_valid);

parameter Name = "";
parameter Width = 8;

input wire clk;
input wire resetn;

input wire [Width-1:0] write_req;
input wire             write_req_valid;
output wire            write_req_bp;

output wire write_resp_valid;
input  wire write_resp_bp;

output wire [Width-1:0] read;
output wire             read_valid;


reg             valid;
reg [Width-1:0] data;

assign read = data;
assign read_valid = 1'b1;

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

endmodule


/**
* Block RAMs of various configurations
*/

module BlockRAM_2RW (clk, resetn,
    port0_req, port0_req_valid, port0_req_bp,
    port0_resp, port0_resp_valid, port0_resp_bp,
    port1_req, port1_req_valid, port1_req_bp,
    port1_resp, port1_resp_valid, port1_resp_bp);

parameter Name = "";
parameter Width = 8;
parameter Depth = 8;
parameter AddrWidth = 8;

input wire clk;
input wire resetn;

reg [Width - 1 : 0] mem [Depth - 1 : 0];

/* Port 0 */
input wire [(Width + AddrWidth + 1) - 1 : 0] port0_req;
input wire                                   port0_req_valid;
output wire                                  port0_req_bp;

wire                     port0_wr   = port0_req[0];
wire [Width - 1 : 0]     port0_data = port0_req[Width : 1];
wire [AddrWidth - 1 : 0] port0_addr = port0_req[Width + AddrWidth : Width + 1];

output reg [Width - 1 : 0] port0_resp;
output reg                 port0_resp_valid;
input  wire                port0_resp_bp;

assign port0_req_bp = port0_resp_bp;
assign port0_resp_valid = port0_req_valid;
assign port0_resp   = mem[port0_addr];

always @(posedge clk)
begin
    if (~resetn)
    begin
        // Nothing to do on reset
    end else begin
        if (port0_req_valid && ~port0_req_bp && port0_wr)
        begin
            mem[port0_addr] <= port0_data;
        end
    end
end


/* Port 1 */
input wire [(Width + AddrWidth + 1) - 1 : 0] port1_req;
input wire                                   port1_req_valid;
output wire                                  port1_req_bp;

wire                     port1_wr   = port1_req[0];
wire [Width - 1 : 0]     port1_data = port1_req[Width : 1];
wire [AddrWidth - 1 : 0] port1_addr = port1_req[Width + AddrWidth : Width + 1];

output reg [Width - 1 : 0] port1_resp;
output reg                 port1_resp_valid;
input  wire                port1_resp_bp;


assign port1_req_bp = port1_resp_bp;
assign port1_resp_valid = port1_req_valid;
assign port1_resp   = mem[port1_addr];

always @(posedge clk)
begin
    if (~resetn)
    begin
        // Nothing to do on reset
    end else begin
        if (port1_req_valid && ~port1_req_bp && port1_wr)
        begin
            mem[port1_addr] <= port1_data;
        end
    end
end

endmodule
`default_nettype wire

