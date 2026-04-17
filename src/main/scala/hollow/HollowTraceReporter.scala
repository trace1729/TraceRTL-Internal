package hollow

import chisel3._
import chisel3.util.HasBlackBoxInline

class HollowTraceReporter(params: HollowTopParams) extends BlackBox with HasBlackBoxInline {
  val io = IO(new HollowTraceReporterIO(params))

  setInline("HollowTraceReporter.sv", HollowInlineSv.reporterModuleSource(params))
}