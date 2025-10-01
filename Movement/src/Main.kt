fun main() {
    val simulationTime = 10.0
    val timeStep = 0.5
    val movementThreads = mutableListOf<Thread>()

    val movables: Array<Movable> = arrayOf(
        Human("Иванов Иван Иванович", 25, 1.8),
        Human("Петров Петр Сергеевич", 30, 2.2),
        Driver("Сидоров Сергей Викторович", 35, 2.0)
    )

    var currentTime = 0.0

    println("СИМУЛЯЦИЯ ДВИЖЕНИЯ")
    println("Время симуляции: $simulationTime секунд")
    println("Количество участников: ${movables.size}")

    while (currentTime <= simulationTime) {
        movementThreads.clear()

        println("\nВремя: %.1f сек".format(currentTime))
        println("-".repeat(70))

        for (movable in movables) {
            val thread = Thread {
                movable.move(timeStep)
                println(movable.getInfo())
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

    movables.forEach { movable ->
        println(movable.getFinalPosition())
    }
}