import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.PI
import kotlin.random.Random

class Human(
    private var fullName: String,
    private var age: Int,
    private var speed: Double = 1.0
) {
    private var x: Double = 0.0
    private var y: Double = 0.0

    fun move(dt: Double) {
        val angle = Random.nextDouble(0.0, 2 * PI)
        x += speed * dt * cos(angle)
        y += speed * dt * sin(angle)
    }

    fun getFullName() = fullName
    fun getAge() = age
    fun getSpeed() = speed
    fun getX() = x
    fun getY() = y

    fun getInfo(): String {
        return "${fullName} (${age} лет, скорость: ${"%.1f".format(speed)}) → координаты: (${"%.1f".format(x)}, ${"%.1f".format(y)})"
    }

    fun getFinalPosition(): String {
        return "${fullName}: (${"%.1f".format(x)}, ${"%.1f".format(y)})"
    }
}

fun main() {
    val humans = arrayOf(
        Human("Иванов Иван Иванович", 25, 1.8),
        Human("Петров Петр Сергеевич", 30, 2.2),
        Human("Сидорова Анна Константиновна", 22, 1.5),
        Human("Козлов Дмитрий Михайлович", 35, 2.0),
        Human("Новикова Елена Владимировна", 28, 1.7),
        Human("Морозов Алексей Петрович", 40, 1.9),
        Human("Волкова Ольга Игоревна", 26, 2.1),
        Human("Павлов Сергей Дмитриевич", 33, 1.4),
        Human("Семенова Мария Алексеевна", 29, 1.8),
        Human("Голубев Виктор Николаевич", 31, 2.0),
        Human("Тихонова Ирина Сергеевна", 24, 1.6),
        Human("Федоров Константин Леонидович", 37, 2.2),
        Human("Дмитриева Наталья Васильевна", 27, 1.7),
        Human("Белов Андрей Геннадьевич", 32, 1.9),
        Human("Кузнецова Татьяна Михайловна", 23, 2.1)
    )

    val simulationTime = 10.0
    val timeStep = 0.5
    var currentTime = 0.0

    println("=== СИМУЛЯЦИЯ СЛУЧАЙНОГО БЛУЖДАНИЯ ===")
    println("Время симуляции: $simulationTime секунд")
    println("Количество участников: ${humans.size}")
    println("=".repeat(70))

    while (currentTime <= simulationTime) {
        println("\nВремя: %.1f сек".format(currentTime))
        println("-".repeat(70))

        humans.forEach { human ->
            human.move(timeStep)
            println(human.getInfo())
        }

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

operator fun String.times(n: Int): String = repeat(n)