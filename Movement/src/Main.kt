
=======
fun main() {
>>>>>>> Interfaces
    val simulationTime = 10.0
    val timeStep = 0.5
    val movementThreads = mutableListOf<Thread>()

<<<<<<< HEAD
=======
    val movables: Array<Movable> = arrayOf(
>>>>>>> Interfaces
        Human("Иванов Иван Иванович", 25, 1.8),
        Human("Петров Петр Сергеевич", 30, 2.2),
        Driver("Сидоров Сергей Викторович", 35, 2.0)
    )

    var currentTime = 0.0

    println("СИМУЛЯЦИЯ ДВИЖЕНИЯ")
    println("Время симуляции: $simulationTime секунд")
<<<<<<< HEAD
=======
    println("Количество участников: ${movables.size}")
>>>>>>> Interfaces

    while (currentTime <= simulationTime) {
        movementThreads.clear()

        println("\nВремя: %.1f сек".format(currentTime))
        println("-".repeat(70))

<<<<<<< HEAD
=======
        for (movable in movables) {
            val thread = Thread {
                movable.move(timeStep)
                println(movable.getInfo())
>>>>>>> Interfaces
            }
            thread.start()
            movementThreads.add(thread)
        }

        movementThreads.forEach { it.join() }
=======
>>>>>>> Interfaces
        currentTime += timeStep
        Thread.sleep(500)
    }

    println("\n" + "=".repeat(70))
    println("СИМУЛЯЦИЯ ЗАВЕРШЕНА!")
    println("\nФинальные позиции участников:")
    println("-".repeat(60))

>>>>>>> Interfaces
