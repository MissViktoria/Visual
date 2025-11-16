package com.example.calculator

import android.Manifest
import android.content.pm.PackageManager
import android.media.MediaPlayer
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.*
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import java.io.File
import java.util.concurrent.TimeUnit

class MediaPlayerActivity : AppCompatActivity() {

    private lateinit var playButton: Button
    private lateinit var pauseButton: Button
    private lateinit var prevButton: Button
    private lateinit var nextButton: Button
    private lateinit var backButton: Button
    private lateinit var statusTextView: TextView
    private lateinit var currentTimeTextView: TextView
    private lateinit var totalTimeTextView: TextView
    private lateinit var seekBar: SeekBar
    private lateinit var volumeSeekBar: SeekBar
    private lateinit var tracksListView: ListView

    private var mediaPlayer: MediaPlayer? = null
    private var currentTrackIndex = 0
    private var musicFiles = mutableListOf<File>()
    private val handler = Handler(Looper.getMainLooper())

    private val updateSeekBar = object : Runnable {
        override fun run() {
            updatePlayerProgress()
            handler.postDelayed(this, 1000)
        }
    }

    private val log_tag = "MEDIA_PLAYER"

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted ->
        if (isGranted) {
            Toast.makeText(this, "Доступ разрешен", Toast.LENGTH_SHORT).show()
            playMusic()
        } else {
            Toast.makeText(this, "Доступ запрещен - некоторые функции не работают", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_media_player)
        initializeViews()
        setupButtons()
        setupSeekBars()
        checkPermissions()
    }

    private fun initializeViews() {
        playButton = findViewById(R.id.playButton)
        pauseButton = findViewById(R.id.pauseButton)
        prevButton = findViewById(R.id.prevButton)
        nextButton = findViewById(R.id.nextButton)
        backButton = findViewById(R.id.backButton)
        statusTextView = findViewById(R.id.currentTrackTextView)
        currentTimeTextView = findViewById(R.id.currentTimeTextView)
        totalTimeTextView = findViewById(R.id.totalTimeTextView)
        seekBar = findViewById(R.id.seekBar)
        volumeSeekBar = findViewById(R.id.volumeSeekBar)
        tracksListView = findViewById(R.id.tracksListView)
        playButton.isEnabled = true
        pauseButton.isEnabled = true
    }

    private fun setupButtons() {
        playButton.setOnClickListener {
            startPlayback()
        }
        pauseButton.setOnClickListener {
            pausePlayback()
        }
        prevButton.setOnClickListener {
            playPreviousTrack()
        }
        nextButton.setOnClickListener {
            playNextTrack()
        }
        backButton.setOnClickListener {
            finish()
        }
    }

    private fun setupSeekBars() {
        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    mediaPlayer?.seekTo(progress)
                }
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })

        volumeSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    val volume = progress / 100.0f
                    mediaPlayer?.setVolume(volume, volume)
                }
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
    }

    private fun checkPermissions() {
        val permission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            Manifest.permission.READ_MEDIA_AUDIO
        } else {
            Manifest.permission.READ_EXTERNAL_STORAGE
        }
        if (ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED) {
            playMusic()
        } else {
            requestPermissionLauncher.launch(permission)
        }
    }

    private fun playMusic() {
        val musicPaths = listOf(Environment.getExternalStorageDirectory().path + "/Music"
        )
        musicFiles.clear()
        for (musicPath in musicPaths) {
            Log.d(log_tag, "Checking path: $musicPath")
            val directory = File(musicPath)

            if (directory.exists() && directory.isDirectory()) {
                directory.listFiles()?.forEach {
                    if (it.isFile && isMusicFile(it)) {
                        musicFiles.add(it)
                        Log.d(log_tag, "Found: ${it.name}")
                    }
                }
            }
        }

        if (musicFiles.isNotEmpty()) {
            statusTextView.text = "Найдено ${musicFiles.size} треков"
            setupTracksList()
            enablePlayerControls(true)
        } else {
            statusTextView.text = "Музыкальные файлы не найдены\nДобавьте MP3 файлы в папку Music"
            enablePlayerControls(false)
        }
    }

    private fun isMusicFile(file: File): Boolean {
        val name = file.name.lowercase()
        return name.endsWith(".mp3")
    }

    private fun setupTracksList() {
        val trackNames = musicFiles.map { it.nameWithoutExtension }
        val adapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, trackNames)
        tracksListView.adapter = adapter
        tracksListView.setOnItemClickListener { parent, view, position, id ->
            playTrack(position)
        }
    }

    private fun enablePlayerControls(enabled: Boolean) {
        playButton.isEnabled = enabled
        pauseButton.isEnabled = enabled
        prevButton.isEnabled = enabled
        nextButton.isEnabled = enabled
    }

    private fun playTrack(position: Int) {
        if (position < 0 || position >= musicFiles.size) return

        mediaPlayer?.release()
        mediaPlayer = MediaPlayer()

        try {
            mediaPlayer?.apply {
                setDataSource(musicFiles[position].absolutePath)
                setOnPreparedListener {
                    start()
                    setupSeekBar()
                    updateTrackInfo()
                    handler.post(updateSeekBar)
                }
                setOnCompletionListener {
                    playNextTrack()
                }
                prepareAsync()
            }
            currentTrackIndex = position
        } catch (e: Exception) {
            Log.e(log_tag, "Error playing track: ${e.message}")
            Toast.makeText(this, "Ошибка воспроизведения", Toast.LENGTH_SHORT).show()
        }
    }

    private fun startPlayback() {
        if (musicFiles.isEmpty()) {
            Toast.makeText(this, "Нет доступных треков", Toast.LENGTH_SHORT).show()
            return
        }

        mediaPlayer?.let { player ->
            if (!player.isPlaying) {
                player.start()
                handler.post(updateSeekBar)
            }
        } ?: run {
            playTrack(0)
        }
    }

    private fun pausePlayback() {
        mediaPlayer?.let { player ->
            if (player.isPlaying) {
                player.pause()
                handler.removeCallbacks(updateSeekBar)
            }
        }
    }

    private fun setupSeekBar() {
        mediaPlayer?.let { player ->
            seekBar.max = player.duration
            totalTimeTextView.text = formatTime(player.duration)
        }
    }

    private fun updatePlayerProgress() {
        mediaPlayer?.let { player ->
            if (player.isPlaying) {
                val currentPosition = player.currentPosition
                seekBar.progress = currentPosition
                currentTimeTextView.text = formatTime(currentPosition)
            }
        }
    }

    private fun updateTrackInfo() {
        val currentTrack = musicFiles[currentTrackIndex]
        statusTextView.text = "Сейчас играет:\n${currentTrack.nameWithoutExtension}"
    }

    private fun playPreviousTrack() {
        if (musicFiles.isEmpty()) return
        val newIndex = if (currentTrackIndex > 0) currentTrackIndex - 1 else musicFiles.size - 1
        playTrack(newIndex)
    }

    private fun playNextTrack() {
        if (musicFiles.isEmpty()) return
        val newIndex = if (currentTrackIndex < musicFiles.size - 1) currentTrackIndex + 1 else 0
        playTrack(newIndex)
    }

    private fun formatTime(milliseconds: Int): String {
        val minutes = TimeUnit.MILLISECONDS.toMinutes(milliseconds.toLong())
        val seconds = TimeUnit.MILLISECONDS.toSeconds(milliseconds.toLong()) -
                TimeUnit.MINUTES.toSeconds(minutes)
        return String.format("%02d:%02d", minutes, seconds)
    }

    override fun onPause() {
        super.onPause()
        mediaPlayer?.let { player ->
            if (player.isPlaying) {
                player.pause()
            }
        }
        handler.removeCallbacks(updateSeekBar)
    }

    override fun onDestroy() {
        super.onDestroy()
        mediaPlayer?.release()
        mediaPlayer = null
        handler.removeCallbacks(updateSeekBar)
    }
}