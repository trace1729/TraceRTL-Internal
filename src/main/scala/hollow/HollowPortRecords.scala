package hollow

import chisel3._

import scala.collection.immutable.ListMap

// Record is used here instead of Bundle because the fetch/reporter BlackBoxes need
// flat per-slot port names like pc_0/pc_1/..., which are easier to inspect in waveforms
// than nested Vec fields. Bundle would naturally group these as vectors or nested members,
// while Record lets us programmatically create the final flattened port map.
// `elements` is the mandatory field that tells Chisel exactly which named ports belong to
// the Record and in what order they should appear in the generated module interface.
class HollowTraceFetchIO(val params: HollowTopParams) extends Record {
  val clock = Input(Clock())
  val reset = Input(Bool())
  val enable = Input(Bool())

  private val validPorts = Seq.tabulate(params.fetchWidth)(_ => Output(Bool()))
  private val pcPorts = Seq.tabulate(params.fetchWidth)(_ => Output(UInt(params.pcWidth.W)))
  private val instrPorts = Seq.tabulate(params.fetchWidth)(_ => Output(UInt(params.instrWidth.W)))
  private val targetPorts = Seq.tabulate(params.fetchWidth)(_ => Output(UInt(params.targetWidth.W)))
  private val exceptionPorts = Seq.tabulate(params.fetchWidth)(_ => Output(UInt(params.controlInfoWidth.W)))
  private val branchTypePorts = Seq.tabulate(params.fetchWidth)(_ => Output(UInt(params.controlInfoWidth.W)))
  private val branchTakenPorts = Seq.tabulate(params.fetchWidth)(_ => Output(UInt(params.controlInfoWidth.W)))
  private val instIdPorts = Seq.tabulate(params.fetchWidth)(_ => Output(UInt(params.instIdWidth.W)))

  override val elements: ListMap[String, Data] = ListMap(
    "clock" -> clock,
    "reset" -> reset,
    "enable" -> enable
  ) ++ ListMap.from(
    (0 until params.fetchWidth).flatMap { slot =>
      Seq(
        s"valid_$slot" -> validPorts(slot),
        s"pc_$slot" -> pcPorts(slot),
        s"instr_$slot" -> instrPorts(slot),
        s"target_$slot" -> targetPorts(slot),
        s"exception_$slot" -> exceptionPorts(slot),
        s"branch_type_$slot" -> branchTypePorts(slot),
        s"branch_taken_$slot" -> branchTakenPorts(slot),
        s"inst_id_$slot" -> instIdPorts(slot)
      )
    }
  )

  def valid(slot: Int): Bool = validPorts(slot)
  def pc(slot: Int): UInt = pcPorts(slot)
  def instr(slot: Int): UInt = instrPorts(slot)
  def target(slot: Int): UInt = targetPorts(slot)
  def exception(slot: Int): UInt = exceptionPorts(slot)
  def branchType(slot: Int): UInt = branchTypePorts(slot)
  def branchTaken(slot: Int): UInt = branchTakenPorts(slot)
  def instId(slot: Int): UInt = instIdPorts(slot)
}

class HollowTraceReporterIO(val params: HollowTopParams) extends Record {
  val clock = Input(Clock())
  val reset = Input(Bool())
  val redirectValid = Input(Bool())
  val redirectInstId = Input(UInt(params.instIdWidth.W))

  private val driveValidPorts = Seq.tabulate(params.issueWidth)(_ => Input(Bool()))
  private val drivePcPorts = Seq.tabulate(params.issueWidth)(_ => Input(UInt(params.pcWidth.W)))
  private val driveInstrPorts = Seq.tabulate(params.issueWidth)(_ => Input(UInt(params.instrWidth.W)))
  private val commitValidPorts = Seq.tabulate(params.issueWidth)(_ => Input(Bool()))
  private val commitPcPorts = Seq.tabulate(params.issueWidth)(_ => Input(UInt(params.pcWidth.W)))
  private val commitInstrPorts = Seq.tabulate(params.issueWidth)(_ => Input(UInt(params.instrWidth.W)))

  override val elements: ListMap[String, Data] = ListMap(
    "clock" -> clock,
    "reset" -> reset,
    "redirect_valid" -> redirectValid,
    "redirect_inst_id" -> redirectInstId
  ) ++ ListMap.from(
    (0 until params.issueWidth).flatMap { slot =>
      Seq(
        s"drive_valid_$slot" -> driveValidPorts(slot),
        s"drive_pc_$slot" -> drivePcPorts(slot),
        s"drive_instr_$slot" -> driveInstrPorts(slot),
        s"commit_valid_$slot" -> commitValidPorts(slot),
        s"commit_pc_$slot" -> commitPcPorts(slot),
        s"commit_instr_$slot" -> commitInstrPorts(slot)
      )
    }
  )

  def driveValid(slot: Int): Bool = driveValidPorts(slot)
  def drivePc(slot: Int): UInt = drivePcPorts(slot)
  def driveInstr(slot: Int): UInt = driveInstrPorts(slot)
  def commitValid(slot: Int): Bool = commitValidPorts(slot)
  def commitPc(slot: Int): UInt = commitPcPorts(slot)
  def commitInstr(slot: Int): UInt = commitInstrPorts(slot)
}