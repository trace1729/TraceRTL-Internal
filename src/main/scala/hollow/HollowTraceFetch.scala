package hollow

import chisel3._
import chisel3.util.HasBlackBoxInline

class HollowTraceFetch(params: HollowTopParams) extends BlackBox with HasBlackBoxInline {
  val io = IO(new HollowTraceFetchIO(params))

  setInline("HollowTraceFetch.sv", HollowInlineSv.fetchModuleSource(params))
}