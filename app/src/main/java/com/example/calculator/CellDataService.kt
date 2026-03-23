package com.example.calculator

import android.Manifest
import android.annotation.SuppressLint
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.os.IBinder
import android.telephony.*
import android.util.Log
import androidx.core.app.ActivityCompat
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import kotlinx.coroutines.*
import org.json.JSONObject
import org.zeromq.ZMQ
import java.util.*
import android.app.usage.NetworkStatsManager
import android.net.NetworkCapabilities
import android.os.Build
import android.app.usage.NetworkStats

class CellDataService : Service(), LocationListener {

    private val TAG = "CellDataService"
    private val SERVER_IP = "192.168.1.42"
    private val SERVER_PORT = "7777"
    private val UPDATE_TIME = 1000L

    private val serviceJob = Job()
    private val serviceScope = CoroutineScope(Dispatchers.IO + serviceJob)

    private lateinit var locationManager: LocationManager
    private lateinit var telephonyManager: TelephonyManager

    private var zmqSocket: ZMQ.Socket? = null
    private var zmqContext: ZMQ.Context? = null
    private var isConnected = false

    private var currentLocation: Location? = null
    private val deviceId = "phone_${UUID.randomUUID().toString().substring(0, 6)}"

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "Service created")

        locationManager = getSystemService(Context.LOCATION_SERVICE) as LocationManager
        telephonyManager = getSystemService(Context.TELEPHONY_SERVICE) as TelephonyManager

        startCollectingData()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "Service started")
        sendStatusToActivity("Сервис запущен")
        return START_STICKY
    }

    private fun startCollectingData() {
        serviceScope.launch {
            connectToServer()

            while (true) {
                if (isConnected && currentLocation != null) {
                    collectAndSendData()
                } else if (!isConnected) {
                    sendStatusToActivity("Нет подключения к серверу")
                    delay(5000)
                } else if (currentLocation == null) {
                    sendStatusToActivity("Ожидание GPS сигнала...")
                    if (checkPermissions()) {
                        try {
                            locationManager.requestLocationUpdates(
                                LocationManager.GPS_PROVIDER,
                                1000L,
                                1f,
                                this@CellDataService
                            )
                        } catch (e: SecurityException) {
                            Log.e(TAG, "SecurityException: ${e.message}")
                            sendStatusToActivity("Нет разрешения на геолокацию")
                        }
                    }
                }
                delay(UPDATE_TIME)
            }
        }
    }

    private fun connectToServer() {
        try {
            zmqContext = ZMQ.context(1)
            zmqSocket = zmqContext?.socket(ZMQ.PUSH)
            val address = "tcp://$SERVER_IP:$SERVER_PORT"
            zmqSocket?.connect(address)
            zmqSocket?.setSendTimeOut(3000)
            isConnected = true
            Log.d(TAG, "Connected to server: $address")
            sendStatusToActivity("Подключено к серверу")
        } catch (e: Exception) {
            Log.e(TAG, "Connection error: ${e.message}")
            isConnected = false
            sendStatusToActivity("Ошибка подключения")
        }
    }

    @SuppressLint("MissingPermission")
    private fun collectAndSendData() {
        try {
            val location = currentLocation ?: return
            val cellInfo = getSimpleCellInfo()

            // Получаем данные о трафике
            val totalTraffic = getTotalTraffic()
            val topApps = getTopAppsByTraffic()

            val jsonData = createJson(location, cellInfo, totalTraffic, topApps)
            zmqSocket?.send(jsonData.toByteArray(), 0)
            sendDataToActivity(jsonData)
        } catch (e: Exception) {
            Log.e(TAG, "Error: ${e.message}")
        }
    }

    private fun sendDataToActivity(jsonData: String) {
        val intent = Intent("CellDataUpdate")
        intent.putExtra("data", jsonData)
        LocalBroadcastManager.getInstance(this).sendBroadcast(intent)
    }

    private fun sendStatusToActivity(status: String) {
        val intent = Intent("CellDataStatus")
        intent.putExtra("status", status)
        LocalBroadcastManager.getInstance(this).sendBroadcast(intent)
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
            createStubJson()
        }
    }

    @SuppressLint("MissingPermission")
    private fun getTotalTraffic(): Pair<Long, Long> {
        var totalTxBytes = 0L
        var totalRxBytes = 0L

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                val networkStatsManager = getSystemService(Context.NETWORK_STATS_SERVICE) as NetworkStatsManager

                // Получаем статистику для мобильной сети
                try {
                    val mobileStats = networkStatsManager.querySummary(
                        NetworkCapabilities.TRANSPORT_CELLULAR,
                        null,
                        0,
                        System.currentTimeMillis()
                    )

                    var bucket: android.app.usage.NetworkStats.Bucket
                    while (mobileStats.hasNextBucket()) {
                        bucket = mobileStats.nextBucket()
                        totalTxBytes += bucket.txBytes
                        totalRxBytes += bucket.rxBytes
                    }
                    mobileStats.close()
                } catch (e: Exception) {
                    Log.e(TAG, "Error getting mobile traffic: ${e.message}")
                }

                // Получаем статистику для Wi-Fi
                try {
                    val wifiStats = networkStatsManager.querySummary(
                        NetworkCapabilities.TRANSPORT_WIFI,
                        null,
                        0,
                        System.currentTimeMillis()
                    )

                    var bucket: android.app.usage.NetworkStats.Bucket
                    while (wifiStats.hasNextBucket()) {
                        bucket = wifiStats.nextBucket()
                        totalTxBytes += bucket.txBytes
                        totalRxBytes += bucket.rxBytes
                    }
                    wifiStats.close()
                } catch (e: Exception) {
                    Log.e(TAG, "Error getting wifi traffic: ${e.message}")
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error getting total traffic: ${e.message}")
        }

        return Pair(totalTxBytes, totalRxBytes)
    }

    @SuppressLint("MissingPermission")
    private fun getTopAppsByTraffic(): List<Pair<String, Long>> {
        return emptyList()
    }

    private fun createLteJson(cell: CellInfoLte): JSONObject {
        val json = JSONObject()
        val identity = JSONObject()
        val signal = JSONObject()

        try {
            val ci = cell.cellIdentity
            val cs = cell.cellSignalStrength

            identity.put("earfcn", try { ci.earfcn } catch (e: Exception) { 0 })
            identity.put("mcc", try { ci.mccString?.toIntOrNull() ?: 0 } catch (e: Exception) { 0 })
            identity.put("mnc", try { ci.mncString?.toIntOrNull() ?: 0 } catch (e: Exception) { 0 })
            identity.put("pci", try { ci.pci } catch (e: Exception) { 0 })
            identity.put("tac", try { ci.tac } catch (e: Exception) { 0 })
            identity.put("cellIdentity", try { ci.ci.toString() } catch (e: Exception) { "" })

            signal.put("asuLevel", try { cs.asuLevel } catch (e: Exception) { 0 })
            signal.put("cqi", try { cs.cqi } catch (e: Exception) { 0 })
            signal.put("rsrp", try { cs.rsrp } catch (e: Exception) { -140 })
            signal.put("rsrq", try { cs.rsrq } catch (e: Exception) { -20 })

            val rssiValue = try { cs.rssi } catch (e: Exception) { -120 }
            signal.put("rssi", if (rssiValue == Integer.MAX_VALUE) -120 else rssiValue)
            signal.put("rssnr", try { cs.rssnr } catch (e: Exception) { 0 })

            json.put("networkType", "LTE")
            json.put("identity", identity)
            json.put("signalStrength", signal)

        } catch (e: Exception) {
            Log.e(TAG, "Error: ${e.message}")
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

            identity.put("bsic", try { ci.bsic } catch (e: Exception) { 0 })
            identity.put("arfcn", try { ci.arfcn } catch (e: Exception) { 0 })
            identity.put("lac", try { ci.lac } catch (e: Exception) { 0 })
            identity.put("mcc", try { ci.mccString?.toIntOrNull() ?: 0 } catch (e: Exception) { 0 })
            identity.put("mnc", try { ci.mncString?.toIntOrNull() ?: 0 } catch (e: Exception) { 0 })
            identity.put("cellIdentity", try { ci.cid.toString() } catch (e: Exception) { "" })

            signal.put("dbm", try { cs.dbm } catch (e: Exception) { -120 })
            signal.put("rssi", try { cs.rssi } catch (e: Exception) { 0 })

            json.put("networkType", "GSM")
            json.put("identity", identity)
            json.put("signalStrength", signal)

        } catch (e: Exception) {
            Log.e(TAG, "Error: ${e.message}")
        }

        return json
    }

    private fun createStubJson(): JSONObject {
        val json = JSONObject()
        val identity = JSONObject()
        val signal = JSONObject()

        identity.put("mcc", 0)
        identity.put("mnc", 0)
        signal.put("rsrp", -140)

        json.put("networkType", "UNKNOWN")
        json.put("identity", identity)
        json.put("signalStrength", signal)

        return json
    }

    private fun createJson(
        location: Location,
        cellInfo: JSONObject,
        totalTraffic: Pair<Long, Long>,
        topApps: List<Pair<String, Long>>
    ): String {
        val json = JSONObject()

        val locationObj = JSONObject().apply {
            put("latitude", location.latitude)
            put("longitude", location.longitude)
            put("altitude", location.altitude)
            put("timestamp", location.time)
            put("accuracy", location.accuracy)
        }

        // Данные о трафике
        val trafficObj = JSONObject().apply {
            put("totalTxBytes", totalTraffic.first)
            put("totalRxBytes", totalTraffic.second)

            val topAppsArray = org.json.JSONArray()
            for (app in topApps) {
                val appObj = JSONObject().apply {
                    put("packageName", app.first)
                    put("bytes", app.second)
                }
                topAppsArray.put(appObj)
            }
            put("topApps", topAppsArray)
        }

        json.put("location", locationObj)
        json.put("cellInfo", cellInfo)
        json.put("traffic", trafficObj)
        json.put("deviceId", deviceId)

        return json.toString()
    }

    private fun checkPermissions(): Boolean {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
                ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED) &&
                ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_PHONE_STATE) == PackageManager.PERMISSION_GRANTED
    }

    override fun onLocationChanged(location: Location) {
        currentLocation = location
    }

    override fun onProviderEnabled(provider: String) {}
    override fun onProviderDisabled(provider: String) {}
    override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}

    override fun onDestroy() {
        super.onDestroy()
        serviceJob.cancel()
        try {
            locationManager.removeUpdates(this)
        } catch (e: SecurityException) {
            Log.e(TAG, "SecurityException in onDestroy: ${e.message}")
        }
        zmqSocket?.close()
        zmqContext?.term()
        sendStatusToActivity("Сервис остановлен")
    }

    override fun onBind(intent: Intent): IBinder? = null
}