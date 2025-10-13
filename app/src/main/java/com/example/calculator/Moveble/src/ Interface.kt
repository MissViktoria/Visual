import kotlin.math.cos
import kotlin.math.sin

interface Movable {
    var x: Double
    var y: Double
    var speed: Double

    fun move(dt: Double)

    fun getInfo(): String
    fun getFinalPosition(): String
}