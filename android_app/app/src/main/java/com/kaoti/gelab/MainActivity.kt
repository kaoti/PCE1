package com.kaoti.gelab

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import android.graphics.Matrix
import android.media.AudioManager
import android.media.ToneGenerator
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import android.util.Base64
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageCapture
import androidx.camera.core.ImageCaptureException
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import com.kaoti.gelab.databinding.ActivityMainBinding
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import kotlin.math.roundToInt

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var cameraExecutor: ExecutorService
    private lateinit var networkExecutor: ExecutorService

    private var imageCapture: ImageCapture? = null
    private val mainHandler = Handler(Looper.getMainLooper())

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(3, TimeUnit.SECONDS)
        .readTimeout(8, TimeUnit.SECONDS)
        .writeTimeout(15, TimeUnit.SECONDS)
        .build()

    private var pendingHash: String? = null
    private var lastAlertedHash: String? = null
    private var isUploading = false
    private var isConfirmationActive = false

    private val pollRunnable = object : Runnable {
        override fun run() {
            pollPendingPhoto()
            mainHandler.postDelayed(this, POLL_INTERVAL_MS)
        }
    }

    private val requestCameraPermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { granted ->
        if (granted) {
            startCamera()
        } else {
            Toast.makeText(
                this,
                "Camera permission is required to photograph tested components.",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        cameraExecutor = Executors.newSingleThreadExecutor()
        networkExecutor = Executors.newSingleThreadExecutor()

        val prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val savedUrl = prefs.getString(KEY_SERVER_URL, getString(R.string.default_server_url))
            ?: getString(R.string.default_server_url)
        binding.etServerUrl.setText(savedUrl)

        binding.btnSaveUrl.setOnClickListener {
            val cleaned = normalizedServerUrl()
            binding.etServerUrl.setText(cleaned)
            prefs.edit().putString(KEY_SERVER_URL, cleaned).apply()
            lastAlertedHash = null
            Toast.makeText(this, "Server URL saved: $cleaned", Toast.LENGTH_SHORT).show()
            pollPendingPhoto()
        }

        binding.btnCapture.setOnClickListener {
            captureAndUploadPhoto()
        }

        binding.btnSkip.setOnClickListener {
            val hash = pendingHash ?: return@setOnClickListener
            skipPendingPhoto(hash)
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
            == PackageManager.PERMISSION_GRANTED
        ) {
            startCamera()
        } else {
            requestCameraPermission.launch(Manifest.permission.CAMERA)
        }
    }

    override fun onResume() {
        super.onResume()
        mainHandler.removeCallbacks(pollRunnable)
        mainHandler.post(pollRunnable)
    }

    override fun onPause() {
        super.onPause()
        mainHandler.removeCallbacks(pollRunnable)
    }

    override fun onDestroy() {
        super.onDestroy()
        cameraExecutor.shutdown()
        networkExecutor.shutdown()
    }

    private fun normalizedServerUrl(): String {
        var raw = binding.etServerUrl.text.toString().trim().trimEnd('/')
        if (!raw.startsWith("http://") && !raw.startsWith("https://")) {
            raw = "http://$raw"
        }
        return raw
    }

    private fun startCamera() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(this)
        cameraProviderFuture.addListener({
            val cameraProvider = cameraProviderFuture.get()
            val preview = Preview.Builder().build().also {
                it.setSurfaceProvider(binding.previewView.surfaceProvider)
            }
            imageCapture = ImageCapture.Builder()
                .setCaptureMode(ImageCapture.CAPTURE_MODE_MINIMIZE_LATENCY)
                .build()

            try {
                cameraProvider.unbindAll()
                cameraProvider.bindToLifecycle(
                    this,
                    CameraSelector.DEFAULT_BACK_CAMERA,
                    preview,
                    imageCapture
                )
            } catch (exc: Exception) {
                Toast.makeText(this, "Camera init failed: ${exc.message}", Toast.LENGTH_LONG).show()
            }
        }, ContextCompat.getMainExecutor(this))
    }

    private fun pollPendingPhoto() {
        if (isUploading) return
        val baseUrl = normalizedServerUrl()
        networkExecutor.execute {
            try {
                val req = Request.Builder()
                    .url("$baseUrl/api/pending-photo")
                    .get()
                    .build()
                httpClient.newCall(req).execute().use { resp ->
                    if (!resp.isSuccessful) throw IllegalStateException("HTTP ${resp.code}")
                    val bodyStr = resp.body?.string() ?: "{}"
                    val json = JSONObject(bodyStr)
                    val pendingObj = if (json.isNull("pending")) null else json.optJSONObject("pending")
                    mainHandler.post {
                        binding.tvConnectionStatus.text = "ONLINE"
                        binding.tvConnectionStatus.setTextColor(getColor(R.color.green_ok))
                        handlePendingUpdate(pendingObj)
                    }
                }
            } catch (exc: Exception) {
                mainHandler.post {
                    binding.tvConnectionStatus.text = "OFFLINE"
                    binding.tvConnectionStatus.setTextColor(getColor(R.color.red_err))
                }
            }
        }
    }

    private fun handlePendingUpdate(pending: JSONObject?) {
        if (isUploading || isConfirmationActive) return

        if (pending == null) {
            pendingHash = null
            binding.btnCapture.isEnabled = false
            binding.btnSkip.isEnabled = false
            binding.overlayBanner.setBackgroundColor(Color.parseColor("#D9161C2B"))
            binding.tvPromptTitle.text = getString(R.string.waiting_prompt)
            binding.tvPromptTitle.setTextColor(getColor(R.color.text_primary))
            binding.tvPromptSub.text = "Ready. Run a test on ESP32-S3 to auto-trigger capture."
            return
        }

        val hash = pending.optString("hash", "")
        if (hash.isEmpty()) return

        pendingHash = hash
        binding.btnCapture.isEnabled = true
        binding.btnSkip.isEnabled = true

        val summary = formatSummaryBanner(pending)
        binding.overlayBanner.setBackgroundColor(Color.parseColor("#E63B2504"))
        binding.tvPromptTitle.text = "SNAP PHOTO FOR: $hash ($summary)"
        binding.tvPromptTitle.setTextColor(Color.parseColor("#FDE68A"))
        val notes = pending.optString("notes", "")
        binding.tvPromptSub.text = if (notes.isNotEmpty() && notes != "null") {
            notes
        } else {
            "Align DUT under camera and tap SNAP & UPLOAD"
        }

        if (hash != lastAlertedHash) {
            lastAlertedHash = hash
            alertUserNewPart()
        }
    }

    private fun formatSummaryBanner(p: JSONObject): String {
        val mat = p.optString("material", "").takeIf { it != "null" } ?: ""
        val pol = p.optString("polarity", "").takeIf { it != "null" } ?: ""
        val dev = p.optString("dev_type", "").takeIf { it != "null" } ?: "DUT"
        val parts = mutableListOf<String>()
        if (mat.isNotEmpty()) parts.add(mat)
        if (pol.isNotEmpty()) parts.add(pol) else parts.add(dev)

        if (!p.isNull("hfe_true")) {
            parts.add("hFE=${p.optDouble("hfe_true").roundToInt()}")
        }
        if (!p.isNull("iceo_uA")) {
            parts.add("Iceo=${p.optDouble("iceo_uA").roundToInt()}uA")
        } else if (!p.isNull("idss_mA")) {
            parts.add("Idss=${String.format("%.2f", p.optDouble("idss_mA"))}mA")
        }
        return parts.joinToString(" ")
    }

    private fun alertUserNewPart() {
        try {
            @Suppress("DEPRECATION")
            val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
            vibrator?.vibrate(VibrationEffect.createOneShot(180, VibrationEffect.DEFAULT_AMPLITUDE))
        } catch (_: Exception) {
        }
        try {
            val tone = ToneGenerator(AudioManager.STREAM_NOTIFICATION, 75)
            tone.startTone(ToneGenerator.TONE_PROP_BEEP, 150)
        } catch (_: Exception) {
        }
    }

    private fun captureAndUploadPhoto() {
        val capture = imageCapture ?: return
        val targetHash = pendingHash ?: return
        if (isUploading) return

        isUploading = true
        binding.btnCapture.isEnabled = false
        binding.btnSkip.isEnabled = false
        binding.tvPromptTitle.text = "Capturing & compressing $targetHash…"

        capture.takePicture(
            cameraExecutor,
            object : ImageCapture.OnImageCapturedCallback() {
                override fun onCaptureSuccess(image: ImageProxy) {
                    try {
                        val base64Jpeg = compressImageProxyToBase64(image)
                        image.close()
                        uploadPhotoBase64(targetHash, base64Jpeg)
                    } catch (exc: Exception) {
                        image.close()
                        onUploadFailed("Image encode error: ${exc.message}")
                    }
                }

                override fun onError(exception: ImageCaptureException) {
                    onUploadFailed("Camera capture failed: ${exception.message}")
                }
            }
        )
    }

    private fun compressImageProxyToBase64(image: ImageProxy): String {
        val buffer = image.planes[0].buffer
        val bytes = ByteArray(buffer.remaining())
        buffer.get(bytes)

        val decoded = BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
            ?: throw IllegalStateException("Could not decode JPEG frame")

        val rotation = image.imageInfo.rotationDegrees
        val rotated = if (rotation != 0) {
            val matrix = Matrix().apply { postRotate(rotation.toFloat()) }
            Bitmap.createBitmap(decoded, 0, 0, decoded.width, decoded.height, matrix, true)
        } else {
            decoded
        }

        val maxDim = 1280
        val scaled = if (rotated.width > maxDim || rotated.height > maxDim) {
            val ratio = minOf(maxDim.toFloat() / rotated.width, maxDim.toFloat() / rotated.height)
            val w = (rotated.width * ratio).roundToInt()
            val h = (rotated.height * ratio).roundToInt()
            Bitmap.createScaledBitmap(rotated, w, h, true)
        } else {
            rotated
        }

        val out = ByteArrayOutputStream()
        scaled.compress(Bitmap.CompressFormat.JPEG, 85, out)
        return Base64.encodeToString(out.toByteArray(), Base64.NO_WRAP)
    }

    private fun uploadPhotoBase64(hash: String, base64Jpeg: String) {
        val baseUrl = normalizedServerUrl()
        val payload = JSONObject().apply {
            put("image_base64", base64Jpeg)
        }.toString()

        networkExecutor.execute {
            try {
                val body = payload.toRequestBody("application/json; charset=utf-8".toMediaType())
                val req = Request.Builder()
                    .url("$baseUrl/api/tests/$hash/photo")
                    .post(body)
                    .build()
                httpClient.newCall(req).execute().use { resp ->
                    if (!resp.isSuccessful) {
                        throw IllegalStateException("Server returned HTTP ${resp.code}")
                    }
                }
                mainHandler.post {
                    isUploading = false
                    showGreenConfirmation(hash)
                }
            } catch (exc: Exception) {
                onUploadFailed("Upload failed: ${exc.message}")
            }
        }
    }

    private fun skipPendingPhoto(hash: String) {
        val baseUrl = normalizedServerUrl()
        networkExecutor.execute {
            try {
                val emptyBody = "{}".toRequestBody("application/json; charset=utf-8".toMediaType())
                val req = Request.Builder()
                    .url("$baseUrl/api/tests/$hash/skip-photo")
                    .post(emptyBody)
                    .build()
                httpClient.newCall(req).execute().close()
                mainHandler.post {
                    pendingHash = null
                    pollPendingPhoto()
                }
            } catch (_: Exception) {
            }
        }
    }

    private fun showGreenConfirmation(hash: String) {
        isConfirmationActive = true
        pendingHash = null
        binding.overlayBanner.setBackgroundColor(Color.parseColor("#E6064E3B"))
        binding.tvPromptTitle.text = "UPLOADED PHOTO FOR $hash!"
        binding.tvPromptTitle.setTextColor(Color.parseColor("#A7F3D0"))
        binding.tvPromptSub.text = "Saved to Ge-LAB database. Resuming live monitor…"
        mainHandler.postDelayed({
            isConfirmationActive = false
            pollPendingPhoto()
        }, 2200)
    }

    private fun onUploadFailed(msg: String) {
        mainHandler.post {
            isUploading = false
            binding.btnCapture.isEnabled = pendingHash != null
            binding.btnSkip.isEnabled = pendingHash != null
            Toast.makeText(this, msg, Toast.LENGTH_LONG).show()
            pollPendingPhoto()
        }
    }

    companion object {
        private const val PREFS_NAME = "kaoti_gelab_prefs"
        private const val KEY_SERVER_URL = "server_url"
        private const val POLL_INTERVAL_MS = 2000L
    }
}
