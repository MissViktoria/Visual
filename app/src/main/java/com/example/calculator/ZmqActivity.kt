package com.example.calculator

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import org.zeromq.SocketType
import org.zeromq.ZMQ
import org.zeromq.ZContext
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class ZmqActivity : AppCompatActivity() {

    private lateinit var btnStartClient: Button
    private lateinit var tvStatus: TextView
    private lateinit var tvLog: TextView
    private lateinit var btnBack: Button

    private val handler = Handler(Looper.getMainLooper())
    private val logTag = "ZMQ_ACTIVITY"
    private var isRunning = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_zmq)

        initViews()
        setupListeners()
    }

    private fun initViews() {
        btnStartClient = findViewById(R.id.btn_start_client)
        tvStatus = findViewById(R.id.tv_status)
        tvLog = findViewById(R.id.tv_log)
        btnBack = findViewById(R.id.btn_back)
    }

    private fun setupListeners() {
        btnBack.setOnClickListener { finish() }

        btnStartClient.setOnClickListener {
            if (!isRunning) {
                startZmqClient()
            }
        }
    }

    private fun startZmqClient() {
        isRunning = true
        tvStatus.text = "Клиент запущен..."

        Thread {
            try {
                val context = ZContext()
                val socket = context.createSocket(SocketType.REQ)

                val serverAddress = "tcp://10.116.8.115:5555"
                socket.connect(serverAddress)

                logMessage("Подключение к серверу: $serverAddress")

                val message = "Hello from Android!"
                socket.send(message.toByteArray(ZMQ.CHARSET), 0)
                logMessage("Отправлено: $message")

                val reply = socket.recv(0)
                val replyStr = String(reply, ZMQ.CHARSET)
                logMessage("Получено: $replyStr")

                handler.post {
                    tvStatus.text = "Получен ответ: $replyStr"
                }

                socket.close()
                context.close()

            } catch (e: Exception) {
                logMessage("Ошибка: ${e.message}")
            } finally {
                isRunning = false
            }
        }.start()
    }

    private fun logMessage(message: String) {
        val time = SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date())
        val logLine = "[$time] $message\n"

        handler.post {
            tvLog.append(logLine)
            Log.d(logTag, message)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        isRunning = false
    }
}