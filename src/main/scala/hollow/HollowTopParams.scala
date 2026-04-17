package hollow

final case class HollowTopParams(
  fetchWidth: Int = 8,
  issueWidth: Int = 4,
  fetchQueueDepth: Int = 32,
  commitQueueDepth: Int = 32,
  fetchLowWatermark: Int = 8,
  pcWidth: Int = 64,
  instrWidth: Int = 32,
  targetWidth: Int = 64,
  controlInfoWidth: Int = 8,
  instIdWidth: Int = 64,
  slotIndexWidth: Int = 8,
  compressedInstructionBytes: Int = 2,
  standardInstructionBytes: Int = 4,
  branchNoneValue: Int = 0
) {
  final val redirectLfsrWidth: Int = 16
  final val redirectCompareBits: Int = 6
  final val redirectCompareValue: Int = 0x3f

  require(fetchWidth > 0, "fetchWidth must be positive")
  require(issueWidth > 0, "issueWidth must be positive")
  require(issueWidth <= fetchWidth, "issueWidth must not exceed fetchWidth")
  require(fetchQueueDepth >= fetchWidth, "fetchQueueDepth must cover at least one fetch batch")
  require(commitQueueDepth >= issueWidth, "commitQueueDepth must cover at least one issue batch")
  require(fetchLowWatermark >= 0, "fetchLowWatermark must be non-negative")
  require(fetchLowWatermark + fetchWidth <= fetchQueueDepth, "fetchLowWatermark must leave room for one refill batch")
  require(redirectCompareBits > 0 && redirectCompareBits <= redirectLfsrWidth, "redirectCompareBits must fit in redirectLfsrWidth")
  require(redirectCompareValue >= 0, "redirectCompareValue must be non-negative")
  require(redirectCompareValue < (1 << redirectCompareBits), "redirectCompareValue must fit in redirectCompareBits")
}

object HollowTopParams {
  val Default: HollowTopParams = HollowTopParams()
}