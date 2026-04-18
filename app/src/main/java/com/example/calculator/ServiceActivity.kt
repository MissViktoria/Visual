package com.example.calculator

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.*

class ServiceActivity : AppCompatActivity() {

    // UI элементы
    private lateinit var tvLatitude: TextView
    private lateinit var tvLongitude: TextView
    private lateinit var tvAltitude: TextView
    private lateinit var tvTime: TextView
    private lateinit var tvNetwork: TextView
    private lateinit var tvSignal: TextView
    private lateinit var tvStatus: TextView
    private lateinit var btnStartService: Button
    private lateinit var btnStopService: Button
    private lateinit var btnForceSend: Button
    private lateinit var btnBack: Button

    private var isServiceRunning = false

    // BroadcastReceiver для получения данных от сервиса
    private val dataReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val data = intent.getStringExtra("data")
            if (data != null) {
                updateUIWithData(data)
            }
        }
    }

    // BroadcastReceiver для статуса сервиса
    private val statusReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val status = intent.getStringExtra("status")
            if (status != null) {
                runOnUiThread {
                    tvStatus.text = "Статус: $status"
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_service)

        initViews()
        setupButtons()
        requestPermissions()

        // Регистрируем receivers
        LocalBroadcastManager.getInstance(this)
            .registerReceiver(dataReceiver, IntentFilter("CellDataUpdate"))
        LocalBroadcastManager.getInstance(this)
            .registerReceiver(statusReceiver, IntentFilter("CellDataStatus"))
    }

    private fun initViews() {
        tvLatitude = findViewById(R.id.tv_latitude)
        tvLongitude = findViewById(R.id.tv_longitude)
        tvAltitude = findViewById(R.id.tv_altitude)
        tvTime = findViewById(R.id.tv_time)
        tvNetwork = findViewById(R.id.tv_network)
        tvSignal = findViewById(R.id.tv_signal)
        tvStatus = findViewById(R.id.tv_status)
        btnStartService = findViewById(R.id.btn_start_service)
        btnStopService = findViewById(R.id.btn_stop_service)
        btnForceSend = findViewById(R.id.btn_force_send)
        btnBack = findViewById(R.id.btn_back)

        btnStopService.isEnabled = false
        btnForceSend.isEnabled = false
    }

    private fun setupButtons() {
        btnBack.setOnClickListener { finish() }

        btnStartService.setOnClickListener {
            if (checkPermissions()) {
                startCellDataService()
            } else {
                requestPermissions()
            }
        }

        btnStopService.setOnClickListener { stopCellDataService() }

        btnForceSend.setOnClickListener { forceSendPendingData() }
    }

    private fun startCellDataService() {
        if (!isServiceRunning) {
            val intent = Intent(this, CellDataService::class.java)
            startService(intent)
            isServiceRunning = true
            btnStartService.isEnabled = false
            btnStopService.isEnabled = true
            btnForceSend.isEnabled = true
            tvStatus.text = "Статус: запуск сервиса..."
            Toast.makeText(this, "Сервис сбора данных запущен", Toast.LENGTH_SHORT).show()
        }
    }

    private fun stopCellDataService() {
        if (isServiceRunning) {
            val intent = Intent(this, CellDataService::class.java)
            stopService(intent)
            isServiceRunning = false
            btnStartService.isEnabled = true
            btnStopService.isEnabled = false
            btnForceSend.isEnabled = false
            tvStatus.text = "Статус: сервис остановлен"
            Toast.makeText(this, "Сервис сбора данных остановлен", Toast.LENGTH_SHORT).show()
        }
    }

    private fun forceSendPendingData() {
        if (!isServiceRunning) {
            Toast.makeText(this, "Сервис не запущен", Toast.LENGTH_SHORT).show()
            return
        }

        val intent = Intent("ForceSendPendingData")
        LocalBroadcastManager.getInstance(this).sendBroadcast(intent)
        Toast.makeText(this, "Принудительная отправка данных из БД...", Toast.LENGTH_SHORT).show()
    }

    private fun updateUIWithData(data: String) {
        try {
            val json = JSONObject(data)

            // Обновляем местоположение
            val location = json.getJSONObject("location")
            runOnUiThread {
                tvLatitude.text = String.format(Locale.US, "%.6f", location.optDouble("latitude", 0.0))
                tvLongitude.text = String.format(Locale.US, "%.6f", location.optDouble("longitude", 0.0))
                tvAltitude.text = String.format(Locale.US, "%.1f м", location.optDouble("altitude", 0.0))

                val time = location.optLong("timestamp", 0)
                if (time > 0) {
                    val date = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
                        .format(Date(time))
                    tvTime.text = date
                } else {
                    tvTime.text = "Н/Д"
                }
            }

            // Обновляем информацию о сети
            val cellInfo = json.getJSONObject("cellInfo")
            val networkType = cellInfo.optString("networkType", "UNKNOWN")

            runOnUiThread {
                tvNetwork.text = networkType

                when (networkType) {
                    "LTE" -> {
                        val signal = cellInfo.optJSONObject("signalStrength")
                        val identity = cellInfo.optJSONObject("identity")

                        if (signal != null && identity != null) {
                            val rsrp = signal.optInt("rsrp", -140)
                            val rsrq = signal.optInt("rsrq", -20)
                            val pci = identity.optInt("pci", 0)
                            val tac = identity.optInt("tac", 0)
                            tvSignal.text = "RSRP: $rsrp dBm | RSRQ: $rsrq dB | PCI: $pci | TAC: $tac"
                        } else {
                            tvSignal.text = "Нет данных о сигнале LTE"
                        }
                    }
                    "GSM" -> {
                        val signal = cellInfo.optJSONObject("signalStrength")
                        val identity = cellInfo.optJSONObject("identity")

                        if (signal != null && identity != null) {
                            val dbm = signal.optInt("dbm", -120)
                            val lac = identity.optInt("lac", 0)
                            tvSignal.text = "Уровень: $dbm dBm | LAC: $lac"
                        } else {
                            tvSignal.text = "Нет данных о сигнале GSM"
                        }
                    }
                    else -> {
                        tvSignal.text = "Нет данных о сигнале (сеть: $networkType)"
                    }
                }
            }

        } catch (e: Exception) {
        }
    }

    private fun requestPermissions() {
        val permissions = arrayOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION,
            Manifest.permission.READ_PHONE_STATE
        )
        ActivityCompat.requestPermissions(this, permissions, 1)
    }

    private fun checkPermissions(): Boolean {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
                ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED) &&
                ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_PHONE_STATE) == PackageManager.PERMISSION_GRANTED
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 1) {
            val allGranted = grantResults.all { it == PackageManager.PERMISSION_GRANTED }
            if (allGranted) {
                startCellDataService()
            } else {
                Toast.makeText(this, "Нужны все разрешения для работы", Toast.LENGTH_LONG).show()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        LocalBroadcastManager.getInstance(this).unregisterReceiver(dataReceiver)
        LocalBroadcastManager.getInstance(this).unregisterReceiver(statusReceiver)
    }
}