package com.example.calculator

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.telephony.*
import android.util.Log
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.*

class LocationActivity : AppCompatActivity(), LocationListener {

    private val TAG = "LocationApp"
    private val SERVER_IP = "10.116.8.115"
    private val SERVER_PORT = "5555"
    private val UPDATE_TIME = 1000L

    private lateinit var tvLat: TextView
    private lateinit var tvLon: TextView
    private lateinit var tvAlt: TextView
    private lateinit var tvTime: TextView
    private lateinit var tvNetwork: TextView
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button
    private lateinit var btnBack: Button
    private lateinit var locationManager: LocationManager
    private lateinit var telephonyManager: TelephonyManager
    private var isSending = false
    private val handler = Handler(Looper.getMainLooper())
    private var currentLocation: Location? = null
    private val deviceId = "phone_${UUID.randomUUID().toString().substring(0, 6)}"
    private var sendRunnable: Runnable? = null
    private var zmqSocket: org.zeromq.ZMQ.Socket? = null
    private var zmqContext: org.zeromq.ZMQ.Context? = null
    private var isConnected = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_location)

        initViews()
        setupButtons()

        locationManager = getSystemService(Context.LOCATION_SERVICE) as LocationManager
        telephonyManager = getSystemService(Context.TELEPHONY_SERVICE) as TelephonyManager

        requestPermissions()
    }

    private fun initViews() {
        tvLat = findViewById(R.id.tv_latitude)
        tvLon = findViewById(R.id.tv_longitude)
        tvAlt = findViewById(R.id.tv_altitude)
        tvTime = findViewById(R.id.tv_time)
        tvNetwork = findViewById(R.id.tv_cell_info)
        btnStart = findViewById(R.id.btn_get_location)
        btnBack = findViewById(R.id.btn_back)

        btnStop = Button(this).apply {
            text = "СТОП"
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = 16
            }
            isEnabled = false
        }

        val layout = findViewById<LinearLayout>(R.id.main)
        layout.addView(btnStop)
    }

    private fun setupButtons() {
        btnBack.setOnClickListener { finish() }
        btnStart.setOnClickListener {
            if (checkPermissions()) {
                if (!isConnected) {
                    connectToServer()
                }
                startSending()
            } else {
                requestPermissions()
            }
        }
        btnStop.setOnClickListener { stopSending() }
    }

    private fun connectToServer() {
        Thread {
            try {
                zmqContext = org.zeromq.ZMQ.context(1)
                zmqSocket = zmqContext?.socket(org.zeromq.ZMQ.PUSH)
                val address = "tcp://$SERVER_IP:$SERVER_PORT"
                zmqSocket?.connect(address)
                zmqSocket?.setSendTimeOut(3000)

                isConnected = true

                runOnUiThread {
                    Toast.makeText(this, "Подключено к серверу", Toast.LENGTH_SHORT).show()
                    tvNetwork.text = "Статус: подключено к серверу"
                }
                Log.d(TAG, "Успешно подключено к серверу: $address")
            } catch (e: Exception) {
                runOnUiThread {
                    Toast.makeText(this, "Ошибка подключения к серверу", Toast.LENGTH_SHORT).show()
                    tvNetwork.text = "Статус: ошибка подключения"
                }
                Log.e(TAG, "Ошибка подключения: ${e.message}")
                isConnected = false
            }
        }.start()
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

    @SuppressLint("MissingPermission")
    private fun startLocationUpdates() {
        try {
            locationManager.removeUpdates(this)

            locationManager.requestLocationUpdates(
                LocationManager.GPS_PROVIDER,
                1000L,
                1f,
                this
            )

            currentLocation = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER)
            if (currentLocation != null) {
                updateUI(currentLocation!!)
            }

        } catch (e: SecurityException) {
            Toast.makeText(this, "Нет разрешения на геолокацию", Toast.LENGTH_LONG).show()
        }
    }

    private fun startSending() {
        if (!isLocationEnabled()) {
            Toast.makeText(this, "Включите GPS", Toast.LENGTH_SHORT).show()
            val intent = Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)
            startActivity(intent)
            return
        }

        if (!isConnected) {
            Toast.makeText(this, "Нет подключения к серверу", Toast.LENGTH_SHORT).show()
            connectToServer()
            return
        }

        if (currentLocation == null) {
            Toast.makeText(this, "Жду GPS сигнал...", Toast.LENGTH_SHORT).show()
            return
        }

        isSending = true
        btnStart.isEnabled = false
        btnStop.isEnabled = true

        sendData()

        sendRunnable = object : Runnable {
            override fun run() {
                if (isSending) {
                    sendData()
                    handler.postDelayed(this, UPDATE_TIME)
                }
            }
        }
        sendRunnable?.let { handler.post(it) }

        Toast.makeText(this, "Отправка начата (1 сек)", Toast.LENGTH_SHORT).show()
        tvNetwork.text = "Статус: отправка данных"
    }

    private fun stopSending() {
        isSending = false
        btnStart.isEnabled = true
        btnStop.isEnabled = false
        sendRunnable?.let { handler.removeCallbacks(it) }
        Toast.makeText(this, "Отправка остановлена", Toast.LENGTH_SHORT).show()
        tvNetwork.text = "Статус: отправка остановлена"
    }

    private fun sendData() {
        Thread {
            try {
                val location = currentLocation ?: return@Thread

                val cellInfo = getSimpleCellInfo()

                val jsonData = createJson(location, cellInfo)

                val success = zmqSocket?.send(jsonData.toByteArray(), 0) ?: false

                runOnUiThread {
                    val time = SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date())
                    if (success) {
                        tvNetwork.text = "Отправлено: $time"
                        Log.d(TAG, "Данные успешно отправлены: $time")
                    } else {
                        tvNetwork.text = "Ошибка отправки: $time"
                        Log.e(TAG, "Не удалось отправить данные")
                        reconnectIfNeeded()
                    }
                }

            } catch (e: Exception) {
                Log.e(TAG, "Ошибка отправки: ${e.message}")
                runOnUiThread {
                    tvNetwork.text = "Ошибка: ${e.message}"
                }
                reconnectIfNeeded()
            }
        }.start()
    }

    private fun reconnectIfNeeded() {
        if (!isConnected) {
            connectToServer()
        }
    }

    @SuppressLint("MissingPermission")
    private fun getSimpleCellInfo(): JSONObject {
        return try {
            val allCells = telephonyManager.allCellInfo

            if (allCells != null && allCells.isNotEmpty()) {
                val firstCell = allCells[0]

                when (firstCell) {
                    is CellInfoLte -> createLteJson(firstCell)
                    is CellInfoGsm -> createGsmJson(firstCell)
                    else -> createStubJson()
                }
            } else {
                createStubJson()
            }

        } catch (e: Exception) {
            Log.e(TAG, "Ошибка получения информации о сети: ${e.message}")
            createStubJson()
        }
    }

    private fun createLteJson(cell: CellInfoLte): JSONObject {
        val json = JSONObject()
        val identity = JSONObject()
        val signal = JSONObject()

        try {
            val ci = cell.cellIdentity
            val cs = cell.cellSignalStrength

            identity.put("cellId", try { ci.ci } catch (e: Exception) { 0 })
            identity.put("mcc", try { ci.mccString?.toIntOrNull() ?: 0 } catch (e: Exception) { 0 })
            identity.put("mnc", try { ci.mncString?.toIntOrNull() ?: 0 } catch (e: Exception) { 0 })

            signal.put("rsrp", cs.rsrp)
            signal.put("dbm", cs.dbm)

            json.put("networkType", "LTE")
            json.put("cellIdentity", identity)
            json.put("signalStrength", signal)

        } catch (e: Exception) {
            Log.e(TAG, "Ошибка создания LTE JSON: ${e.message}")
        }

        return json
    }

    private fun createGsmJson(cell: CellInfoGsm): JSONObject {
        val json = JSONObject()
        val identity = JSONObject()
        val signal = JSONObject()

        try {
            val ci = cell.cellIdentity
            val cs = cell.cellSignalStrength

            identity.put("cellId", ci.cid)
            identity.put("mcc", try { ci.mccString?.toIntOrNull() ?: 0 } catch (e: Exception) { 0 })
            identity.put("mnc", try { ci.mncString?.toIntOrNull() ?: 0 } catch (e: Exception) { 0 })

            signal.put("dbm", cs.dbm)

            json.put("networkType", "GSM")
            json.put("cellIdentity", identity)
            json.put("signalStrength", signal)

        } catch (e: Exception) {
            Log.e(TAG, "Ошибка создания GSM JSON: ${e.message}")
        }

        return json
    }

    private fun createStubJson(): JSONObject {
        val json = JSONObject()
        val identity = JSONObject()
        val signal = JSONObject()

        identity.put("cellId", 0)
        identity.put("mcc", 0)
        identity.put("mnc", 0)

        signal.put("rsrp", 0)
        signal.put("dbm", 0)

        json.put("networkType", "UNKNOWN")
        json.put("cellIdentity", identity)
        json.put("signalStrength", signal)

        return json
    }

    private fun createJson(location: Location, cellInfo: JSONObject): String {
        val json = JSONObject()

        val locationObj = JSONObject().apply {
            put("latitude", location.latitude)
            put("longitude", location.longitude)
            put("altitude", location.altitude)
            put("timestamp", location.time)
            put("speed", location.speed)
            put("accuracy", location.accuracy)
            put("bearing", location.bearing)
        }

        json.put("location", locationObj)
        json.put("cellInfo", cellInfo)
        json.put("deviceId", deviceId)
        json.put("sendTime", System.currentTimeMillis())

        return json.toString()
    }

    private fun isLocationEnabled(): Boolean {
        return locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER) ||
                locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
    }

    override fun onLocationChanged(location: Location) {
        currentLocation = location
        updateUI(location)
    }

    private fun updateUI(location: Location) {
        runOnUiThread {
            tvLat.text = String.format(Locale.US, "Широта: %.6f", location.latitude)
            tvLon.text = String.format(Locale.US, "Долгота: %.6f", location.longitude)
            tvAlt.text = String.format(Locale.US, "Высота: %.1f м", location.altitude)

            val locationTime = Date(location.time)
            val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
            tvTime.text = "Время GPS: ${timeFormat.format(locationTime)}"
        }
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
                startLocationUpdates()
                connectToServer()
            } else {
                Toast.makeText(this, "Нужны все разрешения для работы", Toast.LENGTH_LONG).show()
            }
        }
    }

    override fun onProviderEnabled(provider: String) {
        Log.d(TAG, "GPS включен: $provider")
        runOnUiThread {
            tvNetwork.text = "GPS включен"
        }
    }

    override fun onProviderDisabled(provider: String) {
        Log.d(TAG, "GPS выключен: $provider")
        runOnUiThread {
            tvNetwork.text = "GPS выключен"
            if (isSending) {
                stopSending()
            }
        }
    }

    override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {
    }

    override fun onDestroy() {
        super.onDestroy()
        isSending = false
        handler.removeCallbacksAndMessages(null)
        locationManager.removeUpdates(this)

        try {
            zmqSocket?.close()
            zmqContext?.term()
        } catch (e: Exception) {
            Log.e(TAG, "Ошибка при закрытии ZMQ: ${e.message}")
        }

        Log.d(TAG, "Активность уничтожена")
    }
}