package com.example.calculator.moveble.src

import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.PI
import kotlin.random.Random

open class Human(
    private var fullName: String,
    private var age: Int,
    override var speed: Double = 1.0
) : Movable {
    override var x: Double = 0.0
    override var y: Double = 0.0

    override fun move(dt: Double) {
        val angle = Random.nextDouble(0.0, 2 * PI)
        x += speed * dt * cos(angle)
        y += speed * dt * sin(angle)
    }

    fun getFullName(): String = fullName
    fun getAge(): Int = age

    override fun getInfo(): String {
        return "$fullName (${age} лет, скорость: ${"%.1f".format(speed)}) → координаты: (${"%.1f".format(x)}, ${"%.1f".format(y)})"
    }

    override fun getFinalPosition(): String {
        return "$fullName: (${"%.1f".format(x)}, ${"%.1f".format(y)})"
    }
}