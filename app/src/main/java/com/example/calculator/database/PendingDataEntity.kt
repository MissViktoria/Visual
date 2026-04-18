package com.example.calculator.database

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "pending_data")
data class PendingDataEntity(
    @PrimaryKey(autoGenerate = true)
    val id: Long = 0,
    val jsonData: String,
    val timestamp: Long = System.currentTimeMillis()
)