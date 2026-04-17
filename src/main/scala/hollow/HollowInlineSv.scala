package hollow

object HollowInlineSv {
  private def svPortRange(width: Int): String = if (width == 1) "" else s"[${width - 1}:0]"

  def fetchModuleSource(params: HollowTopParams): String = {
    val portDecls = (0 until params.fetchWidth).flatMap { slot =>
      Seq(
        s"  output wire                       valid_$slot",
        s"  output wire ${svPortRange(params.pcWidth)} pc_$slot",
        s"  output wire ${svPortRange(params.instrWidth)} instr_$slot",
        s"  output wire ${svPortRange(params.targetWidth)} target_$slot",
        s"  output wire ${svPortRange(params.controlInfoWidth)} exception_$slot",
        s"  output wire ${svPortRange(params.controlInfoWidth)} branch_type_$slot",
        s"  output wire ${svPortRange(params.controlInfoWidth)} branch_taken_$slot",
        s"  output wire ${svPortRange(params.instIdWidth)} inst_id_$slot"
      )
    }.mkString(",\n")

    val outputAssigns = (0 until params.fetchWidth).flatMap { slot =>
      Seq(
        s"  assign valid_$slot = valid_q[$slot];",
        s"  assign pc_$slot = pc_q[$slot];",
        s"  assign instr_$slot = instr_q[$slot];",
        s"  assign target_$slot = target_q[$slot];",
        s"  assign exception_$slot = exception_q[$slot];",
        s"  assign branch_type_$slot = branch_type_q[$slot];",
        s"  assign branch_taken_$slot = branch_taken_q[$slot];",
        s"  assign inst_id_$slot = inst_id_q[$slot];"
      )
    }.mkString("\n")

    s"""module HollowTraceFetch(
       |  input  wire                       clock,
       |  input  wire                       reset,
       |  input  wire                       enable,
       |$portDecls
       |);
       |  localparam int FETCH_WIDTH = ${params.fetchWidth};
       |  localparam int PC_WIDTH = ${params.pcWidth};
       |  localparam int INSTR_WIDTH = ${params.instrWidth};
       |  localparam int TARGET_WIDTH = ${params.targetWidth};
      |  localparam int CONTROL_INFO_WIDTH = ${params.controlInfoWidth};
       |  localparam int INST_ID_WIDTH = ${params.instIdWidth};
       |
       |  import "DPI-C" function void trace_read_one_instr(
       |    output longint unsigned pc_va,
       |    output longint unsigned pc_pa,
       |    output longint unsigned memory_addr_va,
       |    output longint unsigned memory_addr_pa,
       |    output longint unsigned target,
       |    output int unsigned instr,
       |    output byte unsigned memory_type,
       |    output byte unsigned memory_size,
       |    output byte unsigned branch_type,
       |    output byte unsigned branch_taken,
       |    output byte unsigned exception,
       |    output byte unsigned fast_simulation,
       |    output longint unsigned instID,
       |    input  byte unsigned idx
       |  );
       |
       |  logic                        valid_q [0:FETCH_WIDTH-1];
       |  logic [PC_WIDTH-1:0]         pc_q [0:FETCH_WIDTH-1];
       |  logic [INSTR_WIDTH-1:0]      instr_q [0:FETCH_WIDTH-1];
       |  logic [TARGET_WIDTH-1:0]     target_q [0:FETCH_WIDTH-1];
      |  logic [CONTROL_INFO_WIDTH-1:0] exception_q [0:FETCH_WIDTH-1];
      |  logic [CONTROL_INFO_WIDTH-1:0] branch_type_q [0:FETCH_WIDTH-1];
      |  logic [CONTROL_INFO_WIDTH-1:0] branch_taken_q [0:FETCH_WIDTH-1];
       |  logic [INST_ID_WIDTH-1:0]    inst_id_q [0:FETCH_WIDTH-1];
       |
       |  longint unsigned pc_va_tmp [0:FETCH_WIDTH-1];
       |  longint unsigned pc_pa_tmp [0:FETCH_WIDTH-1];
       |  longint unsigned memory_addr_va_tmp [0:FETCH_WIDTH-1];
       |  longint unsigned memory_addr_pa_tmp [0:FETCH_WIDTH-1];
       |  longint unsigned target_tmp [0:FETCH_WIDTH-1];
       |  int unsigned     instr_tmp [0:FETCH_WIDTH-1];
       |  byte unsigned    memory_type_tmp [0:FETCH_WIDTH-1];
       |  byte unsigned    memory_size_tmp [0:FETCH_WIDTH-1];
       |  byte unsigned    branch_type_tmp [0:FETCH_WIDTH-1];
       |  byte unsigned    branch_taken_tmp [0:FETCH_WIDTH-1];
       |  byte unsigned    exception_tmp [0:FETCH_WIDTH-1];
       |  byte unsigned    fast_simulation_tmp [0:FETCH_WIDTH-1];
       |  longint unsigned inst_id_tmp [0:FETCH_WIDTH-1];
       |
       |$outputAssigns
       |
       |  always_ff @(posedge clock) begin
       |    if (reset || !enable) begin
       |      for (int slot = 0; slot < FETCH_WIDTH; slot++) begin
       |        valid_q[slot] <= 1'b0;
       |        pc_q[slot] <= '0;
       |        instr_q[slot] <= '0;
       |        target_q[slot] <= '0;
       |        exception_q[slot] <= '0;
       |        branch_type_q[slot] <= '0;
       |        branch_taken_q[slot] <= '0;
       |        inst_id_q[slot] <= '0;
       |      end
       |    end else begin
       |      for (int slot = 0; slot < FETCH_WIDTH; slot++) begin
       |        byte unsigned slot_idx;
       |        slot_idx = byte'(slot);
       |        trace_read_one_instr(
       |          pc_va_tmp[slot],
       |          pc_pa_tmp[slot],
       |          memory_addr_va_tmp[slot],
       |          memory_addr_pa_tmp[slot],
       |          target_tmp[slot],
       |          instr_tmp[slot],
       |          memory_type_tmp[slot],
       |          memory_size_tmp[slot],
       |          branch_type_tmp[slot],
       |          branch_taken_tmp[slot],
       |          exception_tmp[slot],
       |          fast_simulation_tmp[slot],
       |          inst_id_tmp[slot],
       |          slot_idx
       |        );
       |        valid_q[slot] <= inst_id_tmp[slot] != 0;
       |        pc_q[slot] <= pc_va_tmp[slot];
       |        instr_q[slot] <= instr_tmp[slot];
       |        target_q[slot] <= target_tmp[slot];
       |        exception_q[slot] <= exception_tmp[slot];
       |        branch_type_q[slot] <= branch_type_tmp[slot];
       |        branch_taken_q[slot] <= branch_taken_tmp[slot];
       |        inst_id_q[slot] <= inst_id_tmp[slot];
       |      end
       |    end
       |  end
       |endmodule
       |""".stripMargin
  }

  def reporterModuleSource(params: HollowTopParams): String = {
    val portDecls = (0 until params.issueWidth).flatMap { slot =>
      Seq(
        s"  input  wire                       drive_valid_$slot",
        s"  input  wire ${svPortRange(params.pcWidth)} drive_pc_$slot",
        s"  input  wire ${svPortRange(params.instrWidth)} drive_instr_$slot",
        s"  input  wire                       commit_valid_$slot",
        s"  input  wire ${svPortRange(params.pcWidth)} commit_pc_$slot",
        s"  input  wire ${svPortRange(params.instrWidth)} commit_instr_$slot"
      )
    }.mkString(",\n")

    val driveCalls = (0 until params.issueWidth).map { slot =>
      s"""        if (drive_valid_$slot) begin
         |          trace_collect_drive(drive_pc_$slot, drive_instr_$slot, byte'($slot));
         |        end""".stripMargin
    }.mkString("\n")

    val commitCalls = (0 until params.issueWidth).map { slot =>
      s"""        if (commit_valid_$slot) begin
         |          trace_collect_commit(commit_pc_$slot, commit_instr_$slot, 8'd1, byte'($slot));
         |        end""".stripMargin
    }.mkString("\n")

    s"""module HollowTraceReporter(
       |  input  wire                       clock,
       |  input  wire                       reset,
       |  input  wire                       redirect_valid,
       |  input  wire ${svPortRange(params.instIdWidth)} redirect_inst_id,
       |$portDecls
       |);
       |  import "DPI-C" function void trace_redirect(
       |    input longint unsigned inst_id
       |  );
       |
       |  import "DPI-C" function void trace_collect_drive(
       |    input longint unsigned pc,
       |    input int unsigned instr,
       |    input byte unsigned idx
       |  );
       |
       |  import "DPI-C" function void trace_collect_commit(
       |    input longint unsigned pc,
       |    input int unsigned instr,
       |    input byte unsigned instNum,
       |    input byte unsigned idx
       |  );
       |
       |  always_ff @(posedge clock) begin
       |    if (!reset) begin
       |      if (redirect_valid) begin
       |        trace_redirect(redirect_inst_id);
       |      end else begin
       |$driveCalls
       |$commitCalls
       |      end
       |    end
       |  end
       |endmodule
       |""".stripMargin
  }
}