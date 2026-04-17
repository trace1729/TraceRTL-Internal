package hollow

import chisel3._
import chisel3.util._
import _root_.circt.stage.ChiselStage

class HollowTop(params: HollowTopParams = HollowTopParams.Default) extends Module {
  private val zeroEntry = HollowTraceEntry.zero(params)
  private val fetchCountWidth = log2Ceil(params.fetchQueueDepth + 1)
  private val commitCountWidth = log2Ceil(params.commitQueueDepth + 1)
  private val fetchQueueIndexWidth = log2Ceil(params.fetchQueueDepth)
  private val fetchResponseIndexWidth = log2Ceil(params.fetchWidth)
  private val issueIndexWidth = log2Ceil(params.issueWidth)

  private def minUInt(lhs: UInt, rhs: UInt): UInt = Mux(lhs < rhs, lhs, rhs)

  private val fetch = Module(new HollowTraceFetch(params))
  fetch.io.clock := clock
  fetch.io.reset := reset.asBool

  private val reporter = Module(new HollowTraceReporter(params))
  reporter.io.clock := clock
  reporter.io.reset := reset.asBool

  private val fetchQueue = RegInit(VecInit(Seq.fill(params.fetchQueueDepth)(zeroEntry)))
  private val commitQueue = RegInit(VecInit(Seq.fill(params.commitQueueDepth)(zeroEntry)))
  private val fetchCount = RegInit(0.U(fetchCountWidth.W))
  private val commitCount = RegInit(0.U(commitCountWidth.W))

  private val lfsr = RegInit(1.U(params.redirectLfsrWidth.W))
  private val feedback = lfsr(params.redirectLfsrWidth - 1) ^ lfsr(params.redirectLfsrWidth - 3) ^ lfsr(params.redirectLfsrWidth - 4) ^ lfsr(params.redirectLfsrWidth - 6)
  lfsr := Cat(lfsr(params.redirectLfsrWidth - 2, 0), feedback)
  when(lfsr === 0.U) {
    lfsr := 1.U
  }

  private val fetchResponseEntries = Wire(Vec(params.fetchWidth, new HollowTraceEntry(params)))
  private val fetchResponseValid = Seq.tabulate(params.fetchWidth)(slot => fetch.io.valid(slot))
  for (slot <- 0 until params.fetchWidth) {
    fetchResponseEntries(slot) := zeroEntry
    fetchResponseEntries(slot).pc := fetch.io.pc(slot)
    fetchResponseEntries(slot).instr := fetch.io.instr(slot)
    fetchResponseEntries(slot).target := fetch.io.target(slot)
    fetchResponseEntries(slot).exception := fetch.io.exception(slot)
    fetchResponseEntries(slot).branchType := fetch.io.branchType(slot)
    fetchResponseEntries(slot).branchTaken := fetch.io.branchTaken(slot)
    fetchResponseEntries(slot).instId := fetch.io.instId(slot)
  }
  private val fetchResponseCount = PopCount(VecInit(fetchResponseValid))

  private val headValid = commitCount =/= 0.U
  private val headEntry = Wire(new HollowTraceEntry(params))
  headEntry := Mux(headValid, commitQueue(0), zeroEntry)
  private val headException = headValid && (headEntry.exception =/= 0.U)
  private val headBranch = headValid && (headEntry.exception === 0.U) && (headEntry.branchType =/= params.branchNoneValue.U(params.controlInfoWidth.W))
  private val redirectActive = headBranch && (lfsr(params.redirectCompareBits - 1, 0) === params.redirectCompareValue.U(params.redirectCompareBits.W))

  private val issueCount = minUInt(
    fetchCount,
    minUInt(params.issueWidth.U, params.commitQueueDepth.U - commitCount)
  )

  private val commitValidVec = Wire(Vec(params.issueWidth, Bool()))
  private val commitEntries = Wire(Vec(params.issueWidth, new HollowTraceEntry(params)))
  for (slot <- 0 until params.issueWidth) {
    commitValidVec(slot) := false.B
    commitEntries(slot) := zeroEntry
  }

  when(!headException && !redirectActive) {
    for (slot <- 0 until params.issueWidth) {
      val inRange = slot.U < commitCount
      val noException = commitQueue(slot).exception === 0.U
      val priorCommitted = if (slot == 0) true.B else commitValidVec(slot - 1)
      val thisValid = inRange && noException && priorCommitted
      commitValidVec(slot) := thisValid
      commitEntries(slot) := Mux(thisValid, commitQueue(slot), zeroEntry)
    }
  }

  private val commitEmitCount = PopCount(commitValidVec)
  private val commitPopCount = Mux(
    redirectActive,
    0.U(commitCountWidth.W),
    Mux(headException, 1.U(commitCountWidth.W), commitEmitCount)
  )

  private val issueEntries = Wire(Vec(params.issueWidth, new HollowTraceEntry(params)))
  for (slot <- 0 until params.issueWidth) {
    issueEntries(slot) := Mux(slot.U < issueCount, fetchQueue(slot), zeroEntry)
  }

  private val driveValidVec = Seq.tabulate(params.issueWidth)(slot => !redirectActive && (slot.U < issueCount))

  reporter.io.redirectValid := redirectActive
  reporter.io.redirectInstId := headEntry.instId
  for (slot <- 0 until params.issueWidth) {
    reporter.io.driveValid(slot) := driveValidVec(slot)
    reporter.io.drivePc(slot) := issueEntries(slot).pc
    reporter.io.driveInstr(slot) := issueEntries(slot).instr
    reporter.io.commitValid(slot) := commitValidVec(slot)
    reporter.io.commitPc(slot) := commitEntries(slot).pc
    reporter.io.commitInstr(slot) := commitEntries(slot).instr
  }

  private val fetchEnable = !reset.asBool && !redirectActive && (fetchCount <= params.fetchLowWatermark.U)
  fetch.io.enable := fetchEnable

  private val fetchQueueValid = Seq.tabulate(params.fetchQueueDepth)(slot => slot.U < fetchCount)
  private val commitQueueValid = Seq.tabulate(params.commitQueueDepth)(slot => slot.U < commitCount)
  private val checkSequences = !reset.asBool

  HollowAssertions.assertSequence("fetch-response", fetchResponseEntries, fetchResponseValid, params, checkSequences)
  HollowAssertions.assertSequence("fetch-queue", fetchQueue, fetchQueueValid, params, checkSequences)
  HollowAssertions.assertSequence("commit-queue", commitQueue, commitQueueValid, params, checkSequences)

  when(checkSequences) {
    assert(fetchCount <= params.fetchQueueDepth.U, "fetch queue overflow")
    assert(commitCount <= params.commitQueueDepth.U, "commit queue overflow")
  }

  when(reset.asBool || redirectActive) {
    fetchCount := 0.U
    commitCount := 0.U
    for (slot <- 0 until params.fetchQueueDepth) {
      fetchQueue(slot) := zeroEntry
    }
    for (slot <- 0 until params.commitQueueDepth) {
      commitQueue(slot) := zeroEntry
    }
  }.otherwise {
    val fetchBaseCount = fetchCount - issueCount
    val nextFetchCount = fetchBaseCount + fetchResponseCount
    val commitBaseCount = commitCount - commitPopCount
    val nextCommitCount = commitBaseCount + issueCount

    when(checkSequences) {
      assert(nextFetchCount <= params.fetchQueueDepth.U, "fetch queue next state overflow")
      assert(nextCommitCount <= params.commitQueueDepth.U, "commit queue next state overflow")
    }

    fetchCount := nextFetchCount
    for (slot <- 0 until params.fetchQueueDepth) {
      when(slot.U < fetchBaseCount) {
        fetchQueue(slot) := fetchQueue((slot.U + issueCount)(fetchQueueIndexWidth - 1, 0))
      }.elsewhen(slot.U < (fetchBaseCount + fetchResponseCount)) {
        fetchQueue(slot) := fetchResponseEntries((slot.U - fetchBaseCount)(fetchResponseIndexWidth - 1, 0))
      }.otherwise {
        fetchQueue(slot) := zeroEntry
      }
    }

    commitCount := nextCommitCount
    for (slot <- 0 until params.commitQueueDepth) {
      when(slot.U < commitBaseCount) {
        commitQueue(slot) := commitQueue((slot.U + commitPopCount)(fetchQueueIndexWidth - 1, 0))
      }.elsewhen(slot.U < (commitBaseCount + issueCount)) {
        commitQueue(slot) := issueEntries((slot.U - commitBaseCount)(issueIndexWidth - 1, 0))
      }.otherwise {
        commitQueue(slot) := zeroEntry
      }
    }
  }
}

object HollowTop {
  def main(args: Array[String]): Unit = {
    val generatorConfig = HollowGeneratorConfig.parse(args)
    println(
      s"[Hollow] generate HollowTop targetDir=${generatorConfig.targetDir}"
    )
    ChiselStage.emitSystemVerilogFile(
      new HollowTop(generatorConfig.topParams),
      firtoolOpts = Array("--disable-all-randomization", "--strip-debug-info"),
      args = Array("--target-dir", generatorConfig.targetDir, "--split-verilog")
    )
  }
}