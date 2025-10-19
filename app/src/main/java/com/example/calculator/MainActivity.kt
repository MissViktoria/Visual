package com.example.calculator

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import java.util.Locale

class MainActivity : AppCompatActivity() {

    private lateinit var tvScreen: TextView
    private var currentInput = ""
    private var lastCharIsOperator = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        tvScreen = findViewById(R.id.tvScreen)
        tvScreen.text = "0"

        setupAllButtons()
    }

    private fun setupAllButtons() {
        var btn0: Button = findViewById(R.id.btn0)
        btn0.setOnClickListener {
            appendToInput("0")
        }

        var btn1: Button = findViewById(R.id.btn1)
        btn1.setOnClickListener {
            appendToInput("1")
        }

        var btn2: Button = findViewById(R.id.btn2)
        btn2.setOnClickListener {
            appendToInput("2")
        }

        var btn3: Button = findViewById(R.id.btn3)
        btn3.setOnClickListener {
            appendToInput("3")
        }

        var btn4: Button = findViewById(R.id.btn4)
        btn4.setOnClickListener {
            appendToInput("4")
        }

        var btn5: Button = findViewById(R.id.btn5)
        btn5.setOnClickListener {
            appendToInput("5")
        }

        var btn6: Button = findViewById(R.id.btn6)
        btn6.setOnClickListener {
            appendToInput("6")
        }

        var btn7: Button = findViewById(R.id.btn7)
        btn7.setOnClickListener {
            appendToInput("7")
        }

        var btn8: Button = findViewById(R.id.btn8)
        btn8.setOnClickListener {
            appendToInput("8")
        }

        var btn9: Button = findViewById(R.id.btn9)
        btn9.setOnClickListener {
            appendToInput("9")
        }

        var btnPlus: Button = findViewById(R.id.btnPlus)
        btnPlus.setOnClickListener {
            addOperator("+")
        }

        var btnMinus: Button = findViewById(R.id.btnMinus)
        btnMinus.setOnClickListener {
            addOperator("-")
        }

        var btnMultiply: Button = findViewById(R.id.btnMultiply)
        btnMultiply.setOnClickListener {
            addOperator("*")
        }

        var btnDivide: Button = findViewById(R.id.btnDivide)
        btnDivide.setOnClickListener {
            addOperator("/")
        }

        var btnEquals: Button = findViewById(R.id.btnEquals)
        btnEquals.setOnClickListener {
            calculateResult()
        }

        var btnClear: Button = findViewById(R.id.btnClear)
        btnClear.setOnClickListener {
            clearInput()
        }
    }

    private fun appendToInput(value: String) {
        currentInput = if (currentInput == "0" && value != ".") {
            value
        } else {
            currentInput + value
        }
        updateDisplay()
        lastCharIsOperator = false
    }

    private fun addOperator(operator: String) {
        if (currentInput.isEmpty()) {
            if (operator == "-") {
                currentInput = "-"
            }
        } else if (!lastCharIsOperator) {
            currentInput += operator
            lastCharIsOperator = true
        } else {
            if (currentInput.isNotEmpty() && "+-*/".contains(currentInput.last())) {
                currentInput = currentInput.dropLast(1) + operator
            }
        }
        updateDisplay()
    }

    private fun clearInput() {
        currentInput = ""
        lastCharIsOperator = false
        updateDisplay()
    }

    private fun calculateResult() {
        if (currentInput.isEmpty() || lastCharIsOperator) return

        try {
            val result = evaluateExpression(currentInput)
            currentInput = formatResult(result)
            lastCharIsOperator = false
            updateDisplay()
        } catch (e: Exception) {
            currentInput = "Ошибка"
            updateDisplay()
            // Через 2 секунды очищаем ошибку
            tvScreen.postDelayed({
                currentInput = ""
                updateDisplay()
            }, 2000)
        }
    }

    private fun evaluateExpression(expression: String): Double {
        var currentNumber = ""
        var currentOperator = '+'
        var result = 0.0
        var tempResult = 0.0

        for (i in expression.indices) {
            val ch = expression[i]

            if (ch.isDigit() || ch == '.') {
                currentNumber += ch
            }

            if ((!ch.isDigit() && ch != '.') || i == expression.length - 1) {
                if (currentNumber.isNotEmpty()) {
                    val number = currentNumber.toDouble()

                    when (currentOperator) {
                        '+' -> result += tempResult.also { tempResult = number }
                        '-' -> result += tempResult.also { tempResult = -number }
                        '*' -> tempResult *= number
                        '/' -> {
                            if (number == 0.0) throw ArithmeticException("Division by zero")
                            tempResult /= number
                        }
                        else -> tempResult = number
                    }

                    currentNumber = ""
                }

                if (ch in "+-*/") {
                    currentOperator = ch
                }
            }
        }

        result += tempResult
        return result
    }

    private fun formatResult(result: Double): String {
        return if (result == result.toLong().toDouble()) {
            String.format(Locale.getDefault(), "%d", result.toLong())
        } else {
            String.format(Locale.getDefault(), "%.2f", result)
        }
    }

    private fun updateDisplay() {
        val displayText = if (currentInput.isEmpty()) {
            "0"
        } else {
            currentInput
        }
        tvScreen.text = displayText
    }
}