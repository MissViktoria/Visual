package com.example.calculator

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class LocationActivity : AppCompatActivity(), LocationListener {

    companion object {
        private const val LOG_TAG = "LOCATION_ACTIVITY"
        private const val LOCATION_FILE_NAME = "location_history.json"
    }

    private lateinit var locationManager: LocationManager
    private lateinit var tvLatitude: TextView
    private lateinit var tvLongitude: TextView
    private lateinit var tvAltitude: TextView
    private lateinit var tvTime: TextView
    private lateinit var btnBack: Button
    private lateinit var btnGetLocation: Button

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        if (permissions[Manifest.permission.ACCESS_FINE_LOCATION] == true ||
            permissions[Manifest.permission.ACCESS_COARSE_LOCATION] == true) {
            Toast.makeText(this, "Разрешение предоставлено", Toast.LENGTH_LONG).show()
            getLastLocation()
        } else {
            Toast.makeText(this, "Пожалуйста, предоставьте разрешение", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_location)

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }
        initViews()
        setupLocationManager()

        requestPermissionLauncher.launch(
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        )
    }

    private fun initViews() {
        tvLatitude = findViewById(R.id.tv_latitude)
        tvLongitude = findViewById(R.id.tv_longitude)
        tvAltitude = findViewById(R.id.tv_altitude)
        tvTime = findViewById(R.id.tv_time)
        btnBack = findViewById(R.id.btn_back)
        btnGetLocation = findViewById(R.id.btn_get_location)

        btnBack.setOnClickListener {
            finish()
        }

        btnGetLocation.setOnClickListener {
            getLastLocation()
        }

        setInitialState()
    }

    private fun setInitialState() {
        tvLatitude.text = "Ожидание местоположения..."
        tvLongitude.text = "Ожидание местоположения..."
        tvAltitude.text = "Н/Д"
        tvTime.text = "Н/Д"
    }

    private fun setupLocationManager() {
        locationManager = getSystemService(Context.LOCATION_SERVICE) as LocationManager
    }

    private fun getLastLocation() {
        if (!checkPermissions()) {
            requestPermissionLauncher.launch(
                arrayOf(
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_COARSE_LOCATION
                )
            )
            return
        }

        if (!isLocationEnabled()) {
            Toast.makeText(this, "Включите геолокацию в настройках", Toast.LENGTH_SHORT).show()
            val intent = Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)
            startActivity(intent)
            return
        }

        try {
            var location: Location? = null

            if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                location = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER)
            }

            if (location == null && locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                location = locationManager.getLastKnownLocation(LocationManager.NETWORK_PROVIDER)
            }

            if (location != null) {
                Log.d(LOG_TAG, "Получено местоположение: ${location.latitude}, ${location.longitude}")
                updateLocationUI(location)
                saveLocationToFile(location)
            } else {
                Toast.makeText(this, "Местоположение недоступно", Toast.LENGTH_SHORT).show()
                startLocationUpdates()
            }

        } catch (e: SecurityException) {
            Log.e(LOG_TAG, "Ошибка безопасности", e)
            Toast.makeText(this, "Ошибка безопасности", Toast.LENGTH_SHORT).show()
        }
    }

    private fun startLocationUpdates() {
        try {
            if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                locationManager.requestLocationUpdates(
                    LocationManager.GPS_PROVIDER,
                    1000L,
                    1f,
                    this
                )
            }
            if (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                locationManager.requestLocationUpdates(
                    LocationManager.NETWORK_PROVIDER,
                    1000L,
                    1f,
                    this
                )
            }
            Toast.makeText(this, "Поиск местоположения...", Toast.LENGTH_SHORT).show()
        } catch (e: SecurityException) {
            Log.e(LOG_TAG, "Ошибка безопасности при обновлении", e)
        }
    }

    private fun stopLocationUpdates() {
        locationManager.removeUpdates(this)
    }

    private fun checkPermissions(): Boolean {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED ||
                ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED)
    }

    private fun isLocationEnabled(): Boolean {
        return locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER) ||
                locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
    }

    override fun onLocationChanged(location: Location) {
        Log.d(LOG_TAG, "Местоположение изменено: ${location.latitude}, ${location.longitude}")
        updateLocationUI(location)
        saveLocationToFile(location)
        stopLocationUpdates()
    }

    private fun updateLocationUI(location: Location) {
        runOnUiThread {
            tvLatitude.text = "Широта: ${String.format(Locale.US, "%.6f", location.latitude)}"
            tvLongitude.text = "Долгота: ${String.format(Locale.US, "%.6f", location.longitude)}"
            tvAltitude.text = "Высота: ${String.format(Locale.US, "%.2f", location.altitude)} м"

            val locationTime = location.time
            val date = Date(locationTime)
            val timeFormat = SimpleDateFormat("HH:mm:ss dd.MM.yyyy", Locale.getDefault())
            tvTime.text = "Время: ${timeFormat.format(date)}"

            Toast.makeText(this, "Местоположение обновлено!", Toast.LENGTH_SHORT).show()
        }
    }

    private fun saveLocationToFile(location: Location) {
        try {
            val jsonString = """
            {
                "latitude": ${location.latitude},
                "longitude": ${location.longitude},
                "altitude": ${location.altitude},
                "time": ${location.time},
                "provider": "${location.provider}"
            }
            """.trimIndent()

            val file = File(filesDir, LOCATION_FILE_NAME)
            file.appendText("$jsonString\n")

            Log.d(LOG_TAG, "Местоположение сохранено в JSON: $jsonString")
            Toast.makeText(this, "Местоположение сохранено в файл", Toast.LENGTH_SHORT).show()

        } catch (e: Exception) {
            Log.e(LOG_TAG, "Ошибка сохранения местоположения", e)
        }
    }

    override fun onProviderEnabled(provider: String) {
        Log.d(LOG_TAG, "Провайдер включен: $provider")
    }

    override fun onProviderDisabled(provider: String) {
        Log.d(LOG_TAG, "Провайдер отключен: $provider")
    }

    override fun onStatusChanged(provider: String, status: Int, extras: Bundle) {
        Log.d(LOG_TAG, "Статус изменен: $provider, $status")
    }
}