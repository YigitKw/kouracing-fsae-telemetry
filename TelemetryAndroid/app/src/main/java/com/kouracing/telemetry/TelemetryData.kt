package com.kouracing.telemetry

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class TelemetryData(
    val sequenceId: Long = 0,
    val rpm: Int = 0,
    val speed: Int = 0,
    val motorTemp: Int = 0,
    val batteryVoltage: Float = 0f,
    val throttle: Float = 0f,
    val fault: Boolean = false,
    val vehicleState: String = "Ready",
    val inverterStatus: String = "Ready",
    val batteryStatus: String = "Normal"
) {
    fun getThrottlePercent(): Int {
        // Supports both normalized unit range [0.0, 1.0] and pre-scaled integer [0, 100] telemetry feeds
        return if (throttle > 1.0f) {
            throttle.toInt().coerceIn(0, 100)
        } else {
            (throttle * 100).toInt().coerceIn(0, 100)
        }
    }

    companion object {
        const val PACKET_SIZE = 28

        fun fromBinary(packetPayload: ByteArray): TelemetryData? {
            if (packetPayload.size != PACKET_SIZE) return null

            val byteBuffer = ByteBuffer.wrap(packetPayload).order(ByteOrder.LITTLE_ENDIAN)

            // Bitmask ensures unsigned representation when expanding 32-bit integer to Kotlin Long
            val sequenceId = byteBuffer.getInt(0).toLong() and 0xFFFFFFFFL
            val engineRpm = byteBuffer.getFloat(4)
            val vehicleSpeed = byteBuffer.getFloat(8)
            val motorTemperature = byteBuffer.getFloat(12)
            val batteryVoltage = byteBuffer.getFloat(16)
            val throttlePosition = byteBuffer.getFloat(20)
            val faultIndicatorCode = byteBuffer.getInt(24)

            val derivedVehicleState = when {
                faultIndicatorCode == 1 -> "Fault"
                engineRpm > 500f -> "Drive"
                else -> "Ready"
            }

            return TelemetryData(
                sequenceId = sequenceId,
                rpm = engineRpm.toInt(),
                speed = vehicleSpeed.toInt(),
                motorTemp = motorTemperature.toInt(),
                batteryVoltage = batteryVoltage,
                throttle = throttlePosition,
                fault = faultIndicatorCode == 1,
                vehicleState = derivedVehicleState
            )
        }
    }
}
