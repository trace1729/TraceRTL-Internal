package hollow

final case class HollowGeneratorConfig(
  targetDir: String = "build/rtl",
  topParams: HollowTopParams = HollowTopParams.Default
)

object HollowGeneratorConfig {
  private def requireValue(args: Array[String], index: Int, option: String): String = {
    if (index + 1 >= args.length) {
      throw new IllegalArgumentException(s"missing value for $option")
    }
    args(index + 1)
  }

  def printHelp(): Unit = {
    println("Usage: mill standalone.runMain hollow.HollowTop [--target-dir DIR]")
  }

  def parse(args: Array[String]): HollowGeneratorConfig = {
    var parsed = HollowGeneratorConfig()
    var index = 0

    while (index < args.length) {
      args(index) match {
        case "--target-dir" =>
          parsed = parsed.copy(targetDir = requireValue(args, index, "--target-dir"))
          index += 1
        case "--help" | "-h" =>
          printHelp()
          sys.exit(0)
        case ignored if ignored.startsWith("--") =>
          if (index + 1 < args.length && !args(index + 1).startsWith("--")) {
            println(s"[Hollow] ignore option $ignored=${args(index + 1)}")
            index += 1
          } else {
            println(s"[Hollow] ignore flag $ignored")
          }
        case other =>
          println(s"[Hollow] ignore argument $other")
      }
      index += 1
    }

    parsed
  }
}