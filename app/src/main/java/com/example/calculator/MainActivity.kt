package com.example.calculator

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button

class MainActivity : AppCompatActivity() {

    private lateinit var bGoToCalculator: Button
    private lateinit var bGoToPlayerActivity: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bGoToCalculator = findViewById<Button>(R.id.bGoToCalculator)
        bGoToPlayerActivity = findViewById<Button>(R.id.bGoToPlayerActivity)
    }

    override fun onResume() {
        super.onResume()

        bGoToCalculator.setOnClickListener({
            val calculatorIntent = Intent(this, CalculatorActivity::class.java)
            startActivity(calculatorIntent)
        });

        bGoToPlayerActivity.setOnClickListener({
            val playerIntent = Intent(this, MediaPlayerActivity::class.java)
            startActivity(playerIntent)
        });
        }
    }