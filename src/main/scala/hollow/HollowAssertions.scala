package hollow

import chisel3._

object HollowAssertions {
  def assertSequence(name: String, entries: Seq[HollowTraceEntry], valid: Seq[Bool], params: HollowTopParams, enabled: Bool): Unit = {
    for (slot <- 0 until (entries.length - 1)) {
      when(enabled && valid(slot) && valid(slot + 1)) {
        assert(
          entries(slot + 1).instId === (entries(slot).instId + 1.U),
          s"$name instId continuity failed at slot $slot"
        )
        assert(
          entries(slot + 1).pc === HollowTraceMath.expectedNextPc(entries(slot), params),
          s"$name pc continuity failed at slot $slot"
        )
      }
    }
  }
}