package ofdm

import breeze.math.Complex
import chisel3._
import dsptools.numbers._
import dsptools.numbers.implicits._
import org.scalatest.{FlatSpec, Matchers}

class NCOTableSpec extends FlatSpec with Matchers {
  def dut[T <: Data : Ring : BinaryRepresentation : ConvertableTo](params: NCOTableParams[T]): () => NCOTable[T] = () => {
    NCOTableParams.tableNameList.clear()
    new NCOTable(params)
  }

  behavior of "NCO"

  it should "run the tester" in {
    val fixedParams = FixedNCOTableParams(12, 32, 30, 2)
    var arr = Seq[Complex]()
    chisel3.iotesters.Driver.execute(Array("-tbn", "verilator"), dut(fixedParams)) {
      c => new FixedNCOTester(c) { arr = this.sweepPhase() }
    }

    // val xs = arr.indices
    val reals = arr.map(_.real)
    val imags = arr.map(_.imag)

    print("reals = [" + reals.head.toString)
    reals.tail.foreach(x => print(", " + x.toString))
    print("]; ")

    print("imags = [" + imags.head.toString)
    imags.tail.foreach(x => print(", " + x.toString))
    println("];")
    //val plot = Plot().withScatter(xs, reals).withScatter(xs, imags)
    //draw(plot, "NCO Cos")
  }

  ignore should "be accurate to 19 bits" in {
    val fixedParams = FixedNCOTableParams(20, 32, 14, 4)
    var arr = Seq[Complex]()
    chisel3.iotesters.Driver.execute(Array("-tbn", "verilator"), dut(fixedParams)) {
      c => new FixedNCOTester(c) { arr = this.sweepPhase() }
    }

    val reals = arr.map(_.real)
    val imags = arr.map(_.imag)

    print("reals = [" + reals.head.toString)
    reals.tail.foreach(x => print(", " + x.toString))
    print("]; ")

    print("imags = [" + imags.head.toString)
    imags.tail.foreach(x => print(", " + x.toString))
    println("];")


  }

}
