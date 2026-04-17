import mill._
import scalalib._

object standalone extends SbtModule {
  def scalaVersion = "2.13.14"

  override def millSourcePath = os.pwd

  override def ivyDeps = super.ivyDeps() ++ Agg(
    ivy"org.chipsalliance::chisel:6.5.0"
  )

  override def scalacPluginIvyDeps = super.scalacPluginIvyDeps() ++ Agg(
    ivy"org.chipsalliance:::chisel-plugin:6.5.0"
  )

  override def scalacOptions = super.scalacOptions() ++ Seq(
    "-language:reflectiveCalls",
    "-Ymacro-annotations",
    "-Ytasty-reader"
  )
}