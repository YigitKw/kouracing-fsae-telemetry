package com.kouracing.telemetry

import android.os.Bundle
import android.text.InputType
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.google.android.material.materialswitch.MaterialSwitch
import com.google.android.material.textfield.TextInputEditText
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : AppCompatActivity() {

    private lateinit var socketManager: SocketManager

    private lateinit var tvConnectionStatus: TextView
    private lateinit var tvRpmValue: TextView
    private lateinit var progressRpm: ProgressBar
    private lateinit var tvSpeedValue: TextView
    private lateinit var tvTempValue: TextView
    private lateinit var tvVoltValue: TextView
    private lateinit var tvThrottleValue: TextView
    private lateinit var progressThrottle: ProgressBar
    private lateinit var tvStateValue: TextView
    private lateinit var tvInverterValue: TextView
    private lateinit var tvBatteryStatusValue: TextView
    private lateinit var tvFaultValue: TextView

    private lateinit var switchConnectionMode: MaterialSwitch
    private lateinit var layoutLocalMode: LinearLayout
    private lateinit var layoutTunnelMode: LinearLayout
    private lateinit var etIpAddress: TextInputEditText
    private lateinit var etPort: TextInputEditText
    private lateinit var etTunnelUrl: TextInputEditText
    private lateinit var btnConnect: Button
    private lateinit var tvDebugLog: TextView

    private var isConnected = false
    private var isTunnelMode = false

    private val debugLogHistory = mutableListOf<String>()
    private val maxRetainedLogEntries = 30
    private val logTimeFormatter = SimpleDateFormat("HH:mm:ss", Locale.US)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { rootView, windowInsets ->
            val systemBarsInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            rootView.setPadding(
                systemBarsInsets.left,
                systemBarsInsets.top,
                systemBarsInsets.right,
                systemBarsInsets.bottom
            )
            windowInsets
        }

        bindViews()
        configureSocketManager()
        setupConnectionModeSwitch()
        setupConnectButton()
    }

    private fun bindViews() {
        tvConnectionStatus = findViewById(R.id.tvConnectionStatus)
        tvRpmValue = findViewById(R.id.tvRpmValue)
        progressRpm = findViewById(R.id.progressRpm)
        tvSpeedValue = findViewById(R.id.tvSpeedValue)
        tvTempValue = findViewById(R.id.tvTempValue)
        tvVoltValue = findViewById(R.id.tvVoltValue)
        tvThrottleValue = findViewById(R.id.tvThrottleValue)
        progressThrottle = findViewById(R.id.progressThrottle)
        tvStateValue = findViewById(R.id.tvStateValue)
        tvInverterValue = findViewById(R.id.tvInverterValue)
        tvBatteryStatusValue = findViewById(R.id.tvBatteryStatusValue)
        tvFaultValue = findViewById(R.id.tvFaultValue)

        switchConnectionMode = findViewById(R.id.switchConnectionMode)
        layoutLocalMode = findViewById(R.id.layoutLocalMode)
        layoutTunnelMode = findViewById(R.id.layoutTunnelMode)
        etIpAddress = findViewById(R.id.etIpAddress)
        etPort = findViewById(R.id.etPort)
        etTunnelUrl = findViewById(R.id.etTunnelUrl)
        btnConnect = findViewById(R.id.btnConnect)
        tvDebugLog = findViewById(R.id.tvDebugLog)
    }

    private fun configureSocketManager() {
        socketManager = SocketManager()

        socketManager.onTelemetryUpdate = { telemetryData ->
            runOnUiThread { renderTelemetry(telemetryData) }
        }

        socketManager.onRawMessage = { messageText ->
            runOnUiThread { appendDebugLog(messageText) }
        }

        socketManager.onConnectionChange = { connectionState ->
            runOnUiThread { updateConnectionIndicator(connectionState) }
        }

        socketManager.onError = { errorMessage ->
            runOnUiThread {
                appendDebugLog(getString(R.string.format_error, errorMessage))
            }
        }

        socketManager.onAuthRequired = {
            runOnUiThread {
                appendDebugLog(getString(R.string.log_auth_waiting))
                promptTeamAuthentication()
            }
        }

        socketManager.onAuthResult = { authenticated, authError ->
            runOnUiThread {
                if (authenticated) {
                    appendDebugLog(getString(R.string.log_auth_success))
                } else {
                    appendDebugLog(getString(R.string.log_auth_failed, authError ?: "Unknown error"))
                    socketManager.disconnect()
                    updateConnectionIndicator(false)
                }
            }
        }
    }

    private fun setupConnectionModeSwitch() {
        switchConnectionMode.setOnCheckedChangeListener { _, isChecked ->
            isTunnelMode = isChecked
            if (isTunnelMode) {
                layoutLocalMode.visibility = View.GONE
                layoutTunnelMode.visibility = View.VISIBLE
            } else {
                layoutLocalMode.visibility = View.VISIBLE
                layoutTunnelMode.visibility = View.GONE
            }
        }
    }

    private fun setupConnectButton() {
        btnConnect.setOnClickListener {
            if (isConnected) {
                socketManager.disconnect()
                updateConnectionIndicator(false)
            } else {
                if (isTunnelMode) {
                    val targetTunnelUrl = etTunnelUrl.text.toString().trim()

                    if (targetTunnelUrl.isEmpty()) {
                        appendDebugLog(getString(R.string.log_empty_tunnel_url))
                        return@setOnClickListener
                    }

                    appendDebugLog(getString(R.string.format_connecting_tunnel, targetTunnelUrl))
                    updateConnectionIndicator(null)
                    socketManager.connectViaTunnel(tunnelUrl = targetTunnelUrl)
                } else {
                    val targetIp = etIpAddress.text.toString().trim()
                    val targetPortText = etPort.text.toString().trim()
                    val targetPort = targetPortText.toIntOrNull() ?: 3005

                    if (targetIp.isEmpty()) {
                        appendDebugLog(getString(R.string.log_empty_ip))
                        return@setOnClickListener
                    }

                    appendDebugLog(getString(R.string.format_connecting, targetIp, targetPort))
                    updateConnectionIndicator(null)
                    socketManager.connect(targetIp, targetPort)
                }
            }
        }
    }

    private fun promptTeamAuthentication() {
        val passwordInputField = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
            hint = getString(R.string.auth_dialog_message)
            setTextColor(ContextCompat.getColor(context, R.color.text_primary))
            setHintTextColor(ContextCompat.getColor(context, R.color.text_muted))
        }

        val dialogContentLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val paddingPixels = (20 * resources.displayMetrics.density).toInt()
            setPadding(paddingPixels, paddingPixels, paddingPixels, 0)
            addView(passwordInputField)
        }

        AlertDialog.Builder(this, R.style.Theme_KOURacingTelemetry)
            .setTitle(getString(R.string.auth_dialog_title))
            .setMessage(getString(R.string.auth_dialog_message))
            .setView(dialogContentLayout)
            .setCancelable(false)
            .setPositiveButton(getString(R.string.auth_dialog_submit)) { _, _ ->
                val enteredPassword = passwordInputField.text.toString()
                if (enteredPassword.isNotEmpty()) {
                    socketManager.sendTeamAuth(enteredPassword)
                    updateConnectionIndicator(null)
                    tvConnectionStatus.text = getString(R.string.status_authenticating)
                } else {
                    socketManager.disconnect()
                    updateConnectionIndicator(false)
                }
            }
            .setNegativeButton(getString(R.string.auth_dialog_cancel)) { _, _ ->
                socketManager.disconnect()
                updateConnectionIndicator(false)
            }
            .show()
    }

    private fun renderTelemetry(data: TelemetryData) {
        tvRpmValue.text = data.rpm.toString()
        progressRpm.progress = data.rpm

        tvSpeedValue.text = data.speed.toString()

        tvTempValue.text = getString(R.string.format_temp, data.motorTemp)
        val temperatureColorRes = when {
            data.motorTemp >= 85 -> R.color.status_fault
            data.motorTemp >= 75 -> R.color.status_warning
            else -> R.color.accent_orange
        }
        tvTempValue.setTextColor(ContextCompat.getColor(this, temperatureColorRes))

        tvVoltValue.text = String.format(Locale.US, getString(R.string.format_volt), data.batteryVoltage)
        val voltageColorRes = when {
            data.batteryVoltage !in 340f..380f -> R.color.status_fault
            else -> R.color.accent_yellow
        }
        tvVoltValue.setTextColor(ContextCompat.getColor(this, voltageColorRes))

        val throttlePercentage = data.getThrottlePercent()
        tvThrottleValue.text = getString(R.string.format_throttle, throttlePercentage)
        progressThrottle.progress = throttlePercentage

        tvStateValue.text = data.vehicleState
        val vehicleStateColorRes = when (data.vehicleState) {
            "Fault" -> R.color.status_fault
            "Drive" -> R.color.accent_green
            else -> R.color.accent_amber
        }
        tvStateValue.setTextColor(ContextCompat.getColor(this, vehicleStateColorRes))

        tvInverterValue.text = data.inverterStatus
        val inverterStatusColorRes = when (data.inverterStatus) {
            "Overheat", "Redline" -> R.color.status_fault
            "Warning" -> R.color.status_warning
            else -> R.color.status_ok
        }
        tvInverterValue.setTextColor(ContextCompat.getColor(this, inverterStatusColorRes))

        tvBatteryStatusValue.text = data.batteryStatus
        val batteryStatusColorRes = when (data.batteryStatus) {
            "Low Voltage", "High Voltage" -> R.color.status_fault
            else -> R.color.status_ok
        }
        tvBatteryStatusValue.setTextColor(ContextCompat.getColor(this, batteryStatusColorRes))

        if (data.fault) {
            tvFaultValue.text = getString(R.string.fault_active)
            tvFaultValue.setTextColor(ContextCompat.getColor(this, R.color.status_fault))
        } else {
            tvFaultValue.text = getString(R.string.fault_ok)
            tvFaultValue.setTextColor(ContextCompat.getColor(this, R.color.status_ok))
        }
    }

    private fun updateConnectionIndicator(connected: Boolean?) {
        when (connected) {
            true -> {
                isConnected = true
                tvConnectionStatus.text = getString(R.string.status_connected)
                tvConnectionStatus.setTextColor(ContextCompat.getColor(this, R.color.status_connected))
                btnConnect.text = getString(R.string.btn_disconnect)
                appendDebugLog(getString(R.string.log_connected))
            }
            false -> {
                isConnected = false
                tvConnectionStatus.text = getString(R.string.status_disconnected)
                tvConnectionStatus.setTextColor(ContextCompat.getColor(this, R.color.status_disconnected))
                btnConnect.text = getString(R.string.btn_connect)
            }
            null -> {
                tvConnectionStatus.text = getString(R.string.status_connecting)
                tvConnectionStatus.setTextColor(ContextCompat.getColor(this, R.color.accent_amber))
            }
        }
    }

    private fun appendDebugLog(rawMessage: String) {
        val timestamp = logTimeFormatter.format(Date())
        val formattedLogEntry = String.format(Locale.US, "%s  %s", timestamp, rawMessage)

        debugLogHistory.add(0, formattedLogEntry)
        if (debugLogHistory.size > maxRetainedLogEntries) {
            debugLogHistory.removeAt(debugLogHistory.size - 1)
        }

        tvDebugLog.text = debugLogHistory.joinToString("\n")
    }

    override fun onDestroy() {
        super.onDestroy()
        socketManager.disconnect()
    }
}