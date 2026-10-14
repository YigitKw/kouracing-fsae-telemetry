package com.kouracing.telemetry

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class TelemetryDataTest {

    @Test
    fun fromBinary_validPacket_decodesAllFieldsAccurately() {
        val buffer = ByteBuffer.allocate(28).order(ByteOrder.LITTLE_ENDIAN)
        buffer.putInt(1042)        // sequenceId
        buffer.putFloat(5200.0f)   // rpm
        buffer.putFloat(78.5f)     // speed
        buffer.putFloat(62.0f)     // motorTemp
        buffer.putFloat(365.2f)    // batteryVoltage
        buffer.putFloat(0.85f)     // throttle
        buffer.putInt(0)           // fault

        val telemetry = TelemetryData.fromBinary(buffer.array())

        assertNotNull(telemetry)
        assertEquals(1042L, telemetry!!.sequenceId)
        assertEquals(5200, telemetry.rpm)
        assertEquals(78, telemetry.speed)
        assertEquals(62, telemetry.motorTemp)
        assertEquals(365.2f, telemetry.batteryVoltage, 0.01f)
        assertEquals(0.85f, telemetry.throttle, 0.01f)
        assertFalse(telemetry.fault)
        assertEquals("Drive", telemetry.vehicleState)
        assertEquals(85, telemetry.getThrottlePercent())
    }

    @Test
    fun fromBinary_faultCodeSet_setsFaultActive() {
        val buffer = ByteBuffer.allocate(28).order(ByteOrder.LITTLE_ENDIAN)
        buffer.putInt(100)
        buffer.putFloat(0.0f)
        buffer.putFloat(0.0f)
        buffer.putFloat(25.0f)
        buffer.putFloat(350.0f)
        buffer.putFloat(0.0f)
        buffer.putInt(1) // fault active

        val telemetry = TelemetryData.fromBinary(buffer.array())

        assertNotNull(telemetry)
        assertTrue(telemetry!!.fault)
        assertEquals("Fault", telemetry.vehicleState)
    }

    @Test
    fun fromBinary_invalidSize_returnsNull() {
        assertNull(TelemetryData.fromBinary(ByteArray(27)))
        assertNull(TelemetryData.fromBinary(ByteArray(29)))
        assertNull(TelemetryData.fromBinary(ByteArray(0)))
    }

    @Test
    fun getThrottlePercent_handlesNormalizedAndDirectScale() {
        val normalizedData = TelemetryData(throttle = 0.45f)
        assertEquals(45, normalizedData.getThrottlePercent())

        val scaledData = TelemetryData(throttle = 75.0f)
        assertEquals(75, scaledData.getThrottlePercent())

        val overData = TelemetryData(throttle = 120.0f)
        assertEquals(100, overData.getThrottlePercent())
    }
}
