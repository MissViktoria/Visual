import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random
import kotlin.math.PI

class Driver(
    fullName: String,
    age: Int,
    speed: Double,
    private val direction: Double = Random.nextDouble(0.0, 2 * PI)
) : Human(fullName, age, speed) {

    override fun move(dt: Double) {
        super.move(dt)
        x += speed * dt * cos(direction)
        y += speed * dt * sin(direction)
    }
}