module SimJTAG #(
  parameter TICK_DELAY = 50
)(
  input         clock,
  input         reset,
  input         enable,
  input         init_done,
  output        jtag_TCK,
  output        jtag_TMS,
  output        jtag_TDI,
  output        jtag_TRSTn,
  input         jtag_TDO_data,
  input         jtag_TDO_driven,
  output [31:0] exit
);

  assign jtag_TCK = 1'b0;
  assign jtag_TMS = 1'b0;
  assign jtag_TDI = 1'b0;
  assign jtag_TRSTn = 1'b1;
  assign exit = 32'b0;

endmodule