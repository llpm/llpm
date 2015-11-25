/* LLPM Project library file
 *
 * This file contains memory implementations. Registers, block rams, etc..
 */
`default_nettype none

module RTLReg(clk, resetn,
    write_reqs, write_req_valids, write_req_bps,
    write_resp_valids, write_resp_bps,
    read);

parameter Name = "";
parameter Width = 8;
parameter NumWrites = 1;
parameter CLog2NumWrites = 1;
parameter ResetValue = {Width{1'bx}};

input wire clk;
input wire resetn;

input wire [Width-1:0] write_reqs       [NumWrites-1:0];
input wire             write_req_valids [NumWrites-1:0];
output wire            write_req_bps    [NumWrites-1:0];

output wire write_resp_valids [NumWrites-1:0];
input  wire write_resp_bps    [NumWrites-1:0];

output wire [Width-1:0] read;
reg [Width-1:0] data;

assign read = data;

assign write_req_bps = write_resp_bps;
assign write_resp_valids = write_req_valids;

reg has_valid_write;
reg [CLog2NumWrites-1:0] write_select;

integer i;
always@(*)
begin
    write_select = {CLog2NumWrites{1'bx}};
    has_valid_write = 1'b0;
    for (i=0; i<NumWrites; i = i + 1)
    begin
        if (write_req_valids[i] && ! write_resp_bps[i])
        begin
            write_select = i[CLog2NumWrites-1:0];
            has_valid_write = 1'b1;
        end
    end
end

always@(posedge clk)
begin
    if(~resetn)
    begin
        data <= ResetValue;
    end else begin
        if (has_valid_write)
        begin
            data <= write_reqs[write_select];
        end
    end
    `ifdef verilator
    $c("debug_reg(", Name, ", 1, ", data, ");");
    `endif
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

