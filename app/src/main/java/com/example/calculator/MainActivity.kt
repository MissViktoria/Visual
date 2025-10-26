package com.example.calculator

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button

class MainActivity : AppCompatActivity() {

    private lateinit var bGoToCalculator: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bGoToCalculator = findViewById<Button>(R.id.bGoToCalculator)
    }

    override fun onResume() {
        super.onResume()

        bGoToCalculator.setOnClickListener({
            val calculatorIntent = Intent(this, CalculatorActivity::class.java)
            startActivity(calculatorIntent)
        });
    }
}