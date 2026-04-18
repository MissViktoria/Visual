package com.example.calculator

import android.Manifest
import android.annotation.SuppressLint
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
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
import com.example.calculator.database.AppDatabase
import com.example.calculator.database.PendingDataEntity
import kotlinx.coroutines.*
import org.json.JSONObject
import org.zeromq.ZMQ
import java.util.*

class CellDataService : Service(), LocationListener {

    private val TAG = "CellDataService"
    private val SERVER_IP = "192.168.0.107"
    private val SERVER_PORT = "7777"
    private val UPDATE_TIME = 5000L  // Каждые 5 секунд собираем данные
    private val RECONNECT_DELAY = 5000L

    private val serviceJob = Job()
    private val serviceScope = CoroutineScope(Dispatchers.IO + serviceJob)

    private lateinit var locationManager: LocationManager
    private lateinit var telephonyManager: TelephonyManager
    private lateinit var database: AppDatabase

    private var zmqSocket: ZMQ.Socket? = null
    private var zmqContext: ZMQ.Context? = null
    private var isConnected = false
    private var isSendingPendingData = false

    private var currentLocation: Location? = null
    private val deviceId = "phone_${UUID.randomUUID().toString().substring(0, 6)}"

    private val forceSendReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (intent.action == "ForceSendPendingData") {
                Log.d(TAG, "Force send requested by user")
                sendStatusToActivity("Принудительная отправка данных из БД...")
                serviceScope.launch {
                    sendAllPendingDataFromDatabase()
                }
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "Service created")

        locationManager = getSystemService(Context.LOCATION_SERVICE) as LocationManager
        telephonyManager = getSystemService(Context.TELEPHONY_SERVICE) as TelephonyManager
        database = AppDatabase.getInstance(this)

        // Регистрируем receiver для принудительной отправки
        LocalBroadcastManager.getInstance(this).registerReceiver(
            forceSendReceiver,
            IntentFilter("ForceSendPendingData")
        )

        // Проверяем наличие неотправленных данных в БД
        serviceScope.launch {
            val count = database.pendingDataDao().getCount()
            if (count > 0) {
                sendStatusToActivity("В БД есть $count неотправленных данных. Нажмите кнопку для отправки.")
                Log.d(TAG, "Loaded $count pending items from database")
            }
        }

        startCollectingData()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "Service started")
        sendStatusToActivity("Сервис запущен. Данные сохраняются в БД.")
        return START_STICKY
    }

    private fun startCollectingData() {
        serviceScope.launch {
            // Подключаемся к серверу, но не отправляем автоматически
            connectToServer()

            while (true) {
                // ТОЛЬКО СОБИРАЕМ И СОХРАНЯЕМ ДАННЫЕ
                // НИКОГДА НЕ ОТПРАВЛЯЕМ АВТОМАТИЧЕСКИ

                if (currentLocation != null) {
                    collectAndSaveData()  // Всегда сохраняем в БД
                } else {
                    sendStatusToActivity("Ожидание GPS сигнала...")
                    if (checkPermissions()) {
                        withContext(Dispatchers.Main) {
                            try {
                                locationManager.requestLocationUpdates(
                                    LocationManager.GPS_PROVIDER,
                                    60000L,  // 1 минута
                                    1f,
                                    this@CellDataService
                                )
                            } catch (e: SecurityException) {
                                Log.e(TAG, "SecurityException: ${e.message}")
                                sendStatusToActivity("Нет разрешения на геолокацию")
                            }
                        }
                    }
                }

                // Периодически проверяем подключение к серверу, но не отправляем
                if (!isConnected) {
                    connectToServer()
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
            sendStatusToActivity("Подключено к серверу. Для отправки нажмите кнопку.")
        } catch (e: Exception) {
            Log.e(TAG, "Connection error: ${e.message}")
            isConnected = false
            sendStatusToActivity("Ошибка подключения к серверу")
        }
    }

    // НОВЫЙ МЕТОД: ТОЛЬКО СОХРАНЯЕТ ДАННЫЕ, НЕ ОТПРАВЛЯЕТ
    @SuppressLint("MissingPermission")
    private fun collectAndSaveData() {
        try {
            val location = currentLocation ?: return
            val cellInfo = getSimpleCellInfo()
            val jsonData = createJson(location, cellInfo)

            // Всегда сохраняем в БД
            serviceScope.launch {
                val entity = PendingDataEntity(jsonData = jsonData)
                database.pendingDataDao().insert(entity)
                val count = database.pendingDataDao().getCount()
                sendStatusToActivity("Сохранено в БД (всего: $count данных)")
                Log.d(TAG, "Data saved to database. Total pending: $count")

                // Также показываем в UI для отладки
                sendDataToActivity(jsonData)
            }

        } catch (e: Exception) {
            Log.e(TAG, "Error collecting data: ${e.message}")
        }
    }

    // Отправляет ВСЕ данные из БД (вызывается только по кнопке)
    private suspend fun sendAllPendingDataFromDatabase() {
        if (isSendingPendingData) {
            Log.d(TAG, "Already sending pending data, skipping")
            return
        }

        if (!isConnected) {
            sendStatusToActivity("Нет подключения к серверу. Невозможно отправить данные.")
            Log.d(TAG, "Cannot send: not connected to server")
            return
        }

        isSendingPendingData = true

        try {
            var pendingCount = database.pendingDataDao().getCount()
            Log.d(TAG, "=== SEND ALL PENDING DATA START ===")
            Log.d(TAG, "Pending count: $pendingCount, isConnected: $isConnected")

            if (pendingCount == 0) {
                sendStatusToActivity("Нет данных для отправки в БД")
                Log.d(TAG, "No pending data to send")
                return
            }

            sendStatusToActivity("Начинаю отправку $pendingCount записей...")

            var totalSent = 0

            while (pendingCount > 0 && isConnected) {
                Log.d(TAG, "Sending batch. Remaining: $pendingCount")

                val pendingBatch = database.pendingDataDao().getBatch()
                if (pendingBatch.isEmpty()) break

                val successfullySent = mutableListOf<Long>()

                for (data in pendingBatch) {
                    try {
                        val sent = zmqSocket?.send(data.jsonData.toByteArray(), ZMQ.DONTWAIT) ?: false
                        if (sent) {
                            successfullySent.add(data.id)
                            sendDataToActivity(data.jsonData)
                            totalSent++
                            Log.d(TAG, " Pending data sent successfully, id: ${data.id}")
                        } else {
                            Log.d(TAG, " Failed to send pending data, stopping batch")
                            break
                        }
                    } catch (e: Exception) {
                        Log.e(TAG, "Error sending pending data: ${e.message}")
                        break
                    }
                }

                if (successfullySent.isNotEmpty()) {
                    database.pendingDataDao().deleteByIds(successfullySent)
                    sendStatusToActivity("Отправлено: $totalSent записей")
                    Log.d(TAG, "Deleted ${successfullySent.size} sent items")
                }

                pendingCount = database.pendingDataDao().getCount()

                if (pendingCount > 0) {
                    delay(500)  // Небольшая задержка между пакетами
                }
            }

            if (totalSent > 0) {
                sendStatusToActivity(" Отправлено $totalSent записей из БД!")
                Log.d(TAG, "=== SEND COMPLETE, Total sent: $totalSent ===")
            } else {
                sendStatusToActivity(" Не удалось отправить данные. Проверьте подключение.")
            }

        } catch (e: Exception) {
            Log.e(TAG, "Error in sendAllPendingDataFromDatabase: ${e.message}")
            sendStatusToActivity("Ошибка при отправке: ${e.message}")
        } finally {
            isSendingPendingData = false
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
            Log.e(TAG, "Error creating LTE JSON: ${e.message}")
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
            Log.e(TAG, "Error creating GSM JSON: ${e.message}")
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

    private fun createJson(location: Location, cellInfo: JSONObject): String {
        val json = JSONObject()

        val locationObj = JSONObject().apply {
            put("latitude", location.latitude)
            put("longitude", location.longitude)
            put("altitude", location.altitude)
            put("timestamp", location.time)  // Время GPS
            put("accuracy", location.accuracy)
        }

        json.put("location", locationObj)
        json.put("cellInfo", cellInfo)
        json.put("deviceId", deviceId)
        json.put("sentTime", System.currentTimeMillis())  // Время сохранения

        return json.toString()
    }

    private fun checkPermissions(): Boolean {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
                ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED) &&
                ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_PHONE_STATE) == PackageManager.PERMISSION_GRANTED
    }

    override fun onLocationChanged(location: Location) {
        currentLocation = location
        Log.d(TAG, "Location updated: ${location.latitude}, ${location.longitude}")
    }

    override fun onProviderEnabled(provider: String) {
        Log.d(TAG, "GPS enabled: $provider")
        sendStatusToActivity("GPS включен")
    }

    override fun onProviderDisabled(provider: String) {
        Log.d(TAG, "GPS disabled: $provider")
        sendStatusToActivity("GPS выключен")
    }

    override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}

    override fun onDestroy() {
        super.onDestroy()

        try {
            LocalBroadcastManager.getInstance(this).unregisterReceiver(forceSendReceiver)
            Log.d(TAG, "ForceSendReceiver unregistered")
        } catch (e: Exception) {
            Log.e(TAG, "Error unregistering receiver: ${e.message}")
        }

        serviceJob.cancel()

        serviceScope.launch {
            val count = database.pendingDataDao().getCount()
            if (count > 0) {
                sendStatusToActivity("Сервис остановлен. В БД: $count неотправленных данных")
                Log.d(TAG, "Service destroyed with $count pending items in database")
            }
        }

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