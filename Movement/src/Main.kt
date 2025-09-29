import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.PI
import kotlin.random.Random

public open class Human(
    private var fullName: String,
    private var age: Int,
    private var speed: Double = 1.0
) {
    private var x: Double = 0.0
    private var y: Double = 0.0

    public open fun move(dt: Double) {
        val angle = Random.nextDouble(0.0, 2 * PI)
        x += speed * dt * cos(angle)
        y += speed * dt * sin(angle)
    }

    public fun getFullName(): String = fullName
    public fun getAge(): Int = age
    public fun getSpeed(): Double = speed
    public fun getX(): Double = x
    public fun getY(): Double = y

    public fun getInfo(): String {
        return "$fullName (${age} лет, скорость: ${"%.1f".format(speed)}) → координаты: (${"%.1f".format(x)}, ${"%.1f".format(y)})"
    }

    public fun getFinalPosition(): String {
        return "$fullName: (${"%.1f".format(x)}, ${"%.1f".format(y)})"
    }
}

public class Driver(
    fullName: String,
    age: Int,
    speed: Double,
    private val direction: Double = Random.nextDouble(0.0, 2 * PI)
) : Human(fullName, age, speed) {

    public override fun move(dt: Double) {
        super.move(dt)
        val currentX = getX()
        val currentY = getY()
        val newX = currentX + getSpeed() * dt * cos(direction)
        val newY = currentY + getSpeed() * dt * sin(direction)
    }
}

public fun main() {
    val simulationTime = 10.0
    val timeStep = 0.5
    val movementThreads = mutableListOf<Thread>()

    val humans = arrayOf(
        Human("Иванов Иван Иванович", 25, 1.8),
        Human("Петров Петр Сергеевич", 30, 2.2),
        Driver("Сидоров Сергей Викторович", 35, 2.0)
    )

    var currentTime = 0.0

    println("СИМУЛЯЦИЯ ДВИЖЕНИЯ")
    println("Время симуляции: $simulationTime секунд")
    println("Количество участников: ${humans.size}")

    while (currentTime <= simulationTime) {
        movementThreads.clear()

        println("\nВремя: %.1f сек".format(currentTime))
        println("-".repeat(70))

        for (human in humans) {
            val thread = Thread {
                human.move(timeStep)
                println(human.getInfo())
            }
            thread.start()
            movementThreads.add(thread)
        }

        movementThreads.forEach { it.join() }

        currentTime += timeStep
        Thread.sleep(500)
    }

    println("\n" + "=".repeat(70))
    println("СИМУЛЯЦИЯ ЗАВЕРШЕНА!")
    println("\nФинальные позиции участников:")
    println("-".repeat(60))

    humans.forEach { human ->
        println(human.getFinalPosition())
    }
}

