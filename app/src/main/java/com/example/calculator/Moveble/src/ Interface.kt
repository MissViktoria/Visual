package com.example.calculator.moveble.src

interface Movable {
    var x: Double
    var y: Double
    var speed: Double

    fun move(dt: Double)

    fun getInfo(): String
    fun getFinalPosition(): String
}