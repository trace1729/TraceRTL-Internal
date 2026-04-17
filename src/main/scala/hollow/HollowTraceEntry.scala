package hollow

import chisel3._

class HollowTraceEntry(params: HollowTopParams) extends Bundle {
  val pc = UInt(params.pcWidth.W)
  val instr = UInt(params.instrWidth.W)
  val target = UInt(params.targetWidth.W)
  val exception = UInt(params.controlInfoWidth.W)
  val branchType = UInt(params.controlInfoWidth.W)
  val branchTaken = UInt(params.controlInfoWidth.W)
  val instId = UInt(params.instIdWidth.W)
}

object HollowTraceEntry {
  def zero(params: HollowTopParams): HollowTraceEntry = 0.U.asTypeOf(new HollowTraceEntry(params))
}

object HollowTraceMath {
  def instructionBytes(entry: HollowTraceEntry, params: HollowTopParams): UInt = {
    Mux(entry.instr(1, 0) === "b11".U, params.standardInstructionBytes.U(params.pcWidth.W), params.compressedInstructionBytes.U(params.pcWidth.W))
  }

  def isTakenControlFlow(entry: HollowTraceEntry, params: HollowTopParams): Bool = {
    (entry.exception =/= 0.U) || ((entry.branchType =/= params.branchNoneValue.U(params.controlInfoWidth.W)) && (entry.branchTaken =/= 0.U))
  }

  def expectedNextPc(entry: HollowTraceEntry, params: HollowTopParams): UInt = {
    Mux(isTakenControlFlow(entry, params), entry.target, entry.pc + instructionBytes(entry, params))
  }
}