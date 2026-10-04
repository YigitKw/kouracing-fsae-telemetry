package com.kouracing.telemetry

import android.util.Log
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import java.util.concurrent.TimeUnit

class SocketManager {

    companion object {
        private const val TAG = "SocketManager"
        private const val DEFAULT_GATEWAY_IP = "192.168.1.2"
        private const val DEFAULT_GATEWAY_PORT = 3005
        private const val DEFAULT_TUNNEL_URL = "wss://kouracing.motostud.io"
    }

    private var activeWebSocket: WebSocket? = null
    private var httpClient: OkHttpClient? = null

    var onTelemetryUpdate: ((TelemetryData) -> Unit)? = null
    var onRawMessage: ((String) -> Unit)? = null
    var onConnectionChange: ((Boolean) -> Unit)? = null
    var onError: ((String) -> Unit)? = null

    var onAuthRequired: (() -> Unit)? = null
    var onAuthResult: ((Boolean, String?) -> Unit)? = null

    val isConnected: Boolean
        get() = activeWebSocket != null

    fun connect(ip: String = DEFAULT_GATEWAY_IP, port: Int = DEFAULT_GATEWAY_PORT) {
        val targetEndpoint = "ws://$ip:$port"
        initiateSocketSession(targetEndpoint, null, null)
    }

    fun connectViaTunnel(
        tunnelUrl: String = DEFAULT_TUNNEL_URL,
        cfClientId: String? = null,
        cfClientSecret: String? = null
    ) {
        initiateSocketSession(tunnelUrl, cfClientId, cfClientSecret)
    }

    fun sendTeamAuth(password: String) {
        activeWebSocket?.send(password)
    }

    private fun initiateSocketSession(
        endpointUrl: String,
        serviceTokenClientId: String?,
        serviceTokenClientSecret: String?
    ) {
        disconnect()

        try {
            Log.d(TAG, "Initiating connection to $endpointUrl")

            httpClient = OkHttpClient.Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                // Read timeout must remain zero to allow long-lived full-duplex WebSocket communication without idle drop
                .readTimeout(0, TimeUnit.MILLISECONDS)
                .pingInterval(30, TimeUnit.SECONDS)
                .retryOnConnectionFailure(true)
                .build()

            val requestBuilder = Request.Builder().url(endpointUrl)

            if (!serviceTokenClientId.isNullOrBlank() && !serviceTokenClientSecret.isNullOrBlank()) {
                requestBuilder.addHeader("CF-Access-Client-Id", serviceTokenClientId)
                requestBuilder.addHeader("CF-Access-Client-Secret", serviceTokenClientSecret)
            }

            val socketRequest = requestBuilder.build()

            activeWebSocket = httpClient!!.newWebSocket(socketRequest, object : WebSocketListener() {
                override fun onOpen(webSocket: WebSocket, response: Response) {
                    onConnectionChange?.invoke(true)
                    onAuthRequired?.invoke()
                }

                override fun onMessage(webSocket: WebSocket, text: String) {
                    onRawMessage?.invoke(text)

                    when {
                        text == "auth_success" || text.contains("auth_success") -> {
                            onAuthResult?.invoke(true, null)
                        }
                        text.startsWith("auth_error") || text.contains("auth_error") -> {
                            onAuthResult?.invoke(false, text)
                        }
                    }
                }

                override fun onMessage(webSocket: WebSocket, bytes: ByteString) {
                    val decodedTelemetry = TelemetryData.fromBinary(bytes.toByteArray())
                    if (decodedTelemetry != null) {
                        onTelemetryUpdate?.invoke(decodedTelemetry)
                        onRawMessage?.invoke(
                            "RPM:${decodedTelemetry.rpm} SPD:${decodedTelemetry.speed} TEMP:${decodedTelemetry.motorTemp}°C BAT:${decodedTelemetry.batteryVoltage}V"
                        )
                    } else {
                        Log.w(TAG, "Discarded invalid telemetry payload of size: ${bytes.size}")
                    }
                }

                override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                    webSocket.close(1000, null)
                }

                override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                    this@SocketManager.activeWebSocket = null
                    onConnectionChange?.invoke(false)
                }

                override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                    this@SocketManager.activeWebSocket = null
                    onError?.invoke(t.message ?: "Connection failure")
                    onConnectionChange?.invoke(false)
                }
            })

        } catch (e: Exception) {
            onError?.invoke(e.message ?: "Socket initialization failed")
        }
    }

    fun disconnect() {
        activeWebSocket?.close(1000, "Client initiated disconnect")
        activeWebSocket = null
        httpClient?.dispatcher?.executorService?.shutdown()
        httpClient = null
    }
}
