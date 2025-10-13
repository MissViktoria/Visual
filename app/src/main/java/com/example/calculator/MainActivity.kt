package com.example.calculator

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button
import android.widget.TextView

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        var tvScreen: TextView = findViewById(R.id.tvScreen)
        tvScreen.text = "0"
        setupButtons(tvScreen)
    }

    private fun setupButtons(tvScreen: TextView) {
        var firstNumber = ""
        var secondNumber = ""
        var operator = ""
        var isFirstNumber = true

        var btn0: Button = findViewById(R.id.btn0)
        btn0.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "0"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "0"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn1: Button = findViewById(R.id.btn1)
        btn1.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "1"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "1"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn2: Button = findViewById(R.id.btn2)
        btn2.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "2"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "2"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn3: Button = findViewById(R.id.btn3)
        btn3.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "3"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "3"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn4: Button = findViewById(R.id.btn4)
        btn4.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "4"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "4"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn5: Button = findViewById(R.id.btn5)
        btn5.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "5"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "5"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn6: Button = findViewById(R.id.btn6)
        btn6.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "6"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "6"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn7: Button = findViewById(R.id.btn7)
        btn7.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "7"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "7"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn8: Button = findViewById(R.id.btn8)
        btn8.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "8"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "8"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btn9: Button = findViewById(R.id.btn9)
        btn9.setOnClickListener({
            if (isFirstNumber) {
                firstNumber += "9"
                tvScreen.text = firstNumber
            } else {
                secondNumber += "9"
                tvScreen.text = "$firstNumber $operator $secondNumber"
            }
        })

        var btnPlus: Button = findViewById(R.id.btnPlus)
        btnPlus.setOnClickListener({
            if (firstNumber.isNotEmpty()) {
                operator = "+"
                isFirstNumber = false
                tvScreen.text = "$firstNumber $operator"
            }
        })

        var btnMinus: Button = findViewById(R.id.btnMinus)
        btnMinus.setOnClickListener({
            if (firstNumber.isNotEmpty()) {
                operator = "-"
                isFirstNumber = false
                tvScreen.text = "$firstNumber $operator"
            }
        })

        var btnMultiply: Button = findViewById(R.id.btnMultiply)
        btnMultiply.setOnClickListener({
            if (firstNumber.isNotEmpty()) {
                operator = "*"
                isFirstNumber = false
                tvScreen.text = "$firstNumber $operator"
            }
        })

        var btnDivide: Button = findViewById(R.id.btnDivide)
        btnDivide.setOnClickListener({
            if (firstNumber.isNotEmpty()) {
                operator = "/"
                isFirstNumber = false
                tvScreen.text = "$firstNumber $operator"
            }
        })

        var btnEquals: Button = findViewById(R.id.btnEquals)
        btnEquals.setOnClickListener({
            if (firstNumber.isNotEmpty() && secondNumber.isNotEmpty() && operator.isNotEmpty()) {
                val num1 = firstNumber.toDouble()
                val num2 = secondNumber.toDouble()
                var result = 0.0

                when (operator) {
                    "+" -> result = num1 + num2
                    "-" -> result = num1 - num2
                    "*" -> result = num1 * num2
                    "/" -> {
                        if (num2 != 0.0) {
                            result = num1 / num2
                        } else {
                            tvScreen.text = "Ошибка: деление на 0"
                            return@setOnClickListener
                        }
                    }
                }
                val resultText = if (result % 1 == 0.0) {
                    result.toInt().toString()
                } else {
                    "%.2f".format(result)
                }

                tvScreen.text = resultText

                firstNumber = resultText
                secondNumber = ""
                operator = ""
                isFirstNumber = true
            }
        })

        var btnClear: Button = findViewById(R.id.btnClear)
        btnClear.setOnClickListener({
            firstNumber = ""
            secondNumber = ""
            operator = ""
            isFirstNumber = true
            tvScreen.text = "0"
        })
    }
}