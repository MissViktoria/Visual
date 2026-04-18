package com.example.calculator.database

import androidx.room.*

@Dao
interface PendingDataDao {
    @Insert
    suspend fun insert(data: PendingDataEntity)

    @Query("SELECT * FROM pending_data ORDER BY timestamp ASC LIMIT 50")
    suspend fun getBatch(): List<PendingDataEntity>

    @Query("DELETE FROM pending_data WHERE id IN (:ids)")
    suspend fun deleteByIds(ids: List<Long>)

    @Query("SELECT COUNT(*) FROM pending_data")
    suspend fun getCount(): Int
}