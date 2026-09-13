package com.meijworks.loofdoes.protocol

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The lines here are copied from the firmware's own native tests
 * (test_RemoteSprayer.cpp), so both ends agree on the same bytes.
 */
class SprayerProtocolTest {

    @Test
    fun status_lineFromFirmwareTest_parsesEveryField() {
        val m = SprayerProtocol.parse("S:1.00,100.0,100.0,1800.0,2048,0,0,0,1843,0,0")
        assertTrue(m is BoardMessage.Status)
        val s = (m as BoardMessage.Status).sample
        assertEquals(1.00f, s.speedMs, 0.001f)
        assertEquals(100.0f, s.requestedLha, 0.001f)
        assertEquals(100.0f, s.actualLha!!, 0.001f)
        assertEquals(1800.0f, s.flowMlMin, 0.001f)
        assertEquals(2048, s.raw)
        assertFalse(s.mixer); assertFalse(s.vernevelaar); assertFalse(s.pump)
        assertEquals(1843, s.pumpPwm)
        assertFalse(s.deviation)
        assertEquals(CalibrationOwner.NONE, s.calibrationOwner)
        assertEquals(3.6f, s.speedKmh, 0.001f)
    }

    @Test
    fun status_protocol2_inputAndOutputBits() {
        val s = (SprayerProtocol.parse("S:0.00,50.0,-1.0,0.0,0,1,0,0,0,0,0,1001,1000") as BoardMessage.Status).sample
        assertEquals(listOf(true, false, false, true), s.inputs)
        assertEquals(listOf(true, false, false, false), s.outputs)
    }

    @Test
    fun status_protocol1_hasNoBits() {
        val s = (SprayerProtocol.parse("S:1.00,100.0,100.0,1800.0,2048,0,0,0,1843,0,0") as BoardMessage.Status).sample
        assertTrue(s.inputs.isEmpty())
        assertTrue(s.outputs.isEmpty())
    }

    @Test
    fun nmea_lineKeepsTheSentenceVerbatim() {
        val m = SprayerProtocol.parse("N:\$GPGGA,123519,4807.038,N,01131.000,E,0,00,,,M,,M,,*47")
        assertEquals(BoardMessage.Nmea("\$GPGGA,123519,4807.038,N,01131.000,E,0,00,,,M,,M,,*47"), m)
        assertEquals("TELEM N 1", SprayerProtocol.cmdTelemetryNmea(true))
    }

    @Test
    fun status_undefinedActual_isNull_andCalibrationOwnerApp() {
        val s = (SprayerProtocol.parse("S:0.00,50.0,-1.0,0.0,0,0,0,0,777,0,2") as BoardMessage.Status).sample
        assertNull(s.actualLha)
        assertEquals(777, s.pumpPwm)
        assertEquals(CalibrationOwner.APP, s.calibrationOwner)
    }

    @Test
    fun status_deviationFlag_andOutputs() {
        val s = (SprayerProtocol.parse("S:2.00,200.0,111.1,7200.0,4095,1,1,1,4095,1,0") as BoardMessage.Status).sample
        assertTrue(s.deviation)
        assertTrue(s.mixer && s.vernevelaar && s.pump)
    }

    @Test
    fun gps_line_andNoFixEver() {
        val g = (SprayerProtocol.parse("G:4,52.500000,6.250000,600") as BoardMessage.Gps).sample
        assertEquals(4, g.quality)
        assertEquals(52.5, g.latitude, 1e-9)
        assertEquals(6.25, g.longitude, 1e-9)
        assertEquals(600L, g.fixAgeMs)
        assertEquals("RTK fixed", g.qualityLabel)

        val none = (SprayerProtocol.parse("G:1,0.000000,0.000000,-1") as BoardMessage.Gps).sample
        assertNull(none.fixAgeMs)

        // The board's parser reports its invalid sentinel until the first fix.
        val sentinel = (SprayerProtocol.parse("G:0,999999.875000,999999.875000,-1") as BoardMessage.Gps).sample
        assertFalse(sentinel.hasPosition)
        assertTrue(g.hasPosition)
        assertEquals("GPS", none.qualityLabel)
    }

    @Test
    fun calibration_lines() {
        val d = SprayerProtocol.parse("C:D,1,2048,100") as BoardMessage.DoseCalPoint
        assertEquals(DosePoint(1, 2048, 100), d.point)
        val p = SprayerProtocol.parse("C:P,2,4095,4000") as BoardMessage.PwmCalPoint
        assertEquals(PwmPoint(2, 4095, 4000), p.point)
    }

    @Test
    fun config_version_countdown_and_acks() {
        assertEquals(BoardMessage.ConfigValue("width_cm", 300), SprayerProtocol.parse("K:width_cm,300"))
        assertEquals(BoardMessage.Version("0.2", 1), SprayerProtocol.parse("V:0.2,1"))
        assertEquals(BoardMessage.RunCountdown(60), SprayerProtocol.parse("R:60"))
        assertEquals(BoardMessage.Ok, SprayerProtocol.parse("OK"))
        assertEquals(BoardMessage.Busy, SprayerProtocol.parse("BUSY"))
        assertEquals(BoardMessage.Error("range"), SprayerProtocol.parse("ERR:range"))
    }

    @Test
    fun garbage_isUnknown_neverThrows() {
        assertTrue(SprayerProtocol.parse("") is BoardMessage.Unknown)
        assertTrue(SprayerProtocol.parse("S:abc") is BoardMessage.Unknown)
        assertTrue(SprayerProtocol.parse("S:1,2") is BoardMessage.Unknown)
        assertTrue(SprayerProtocol.parse("X:1") is BoardMessage.Unknown)
        assertTrue(SprayerProtocol.parse("C:Q,1,2,3") is BoardMessage.Unknown)
        assertTrue(SprayerProtocol.parse("R:x") is BoardMessage.Unknown)
    }

    @Test
    fun trailingCr_isTolerated() {
        assertEquals(BoardMessage.Ok, SprayerProtocol.parse("OK\r"))
    }

    @Test
    fun commands_matchFirmwareGrammar() {
        assertEquals("TELEM S 1", SprayerProtocol.cmdTelemetryStatus(true))
        assertEquals("TELEM G 0", SprayerProtocol.cmdTelemetryGps(false))
        assertEquals("CAL MODE 1", SprayerProtocol.cmdCalMode(true))
        assertEquals("CFG SET width_cm 450", SprayerProtocol.cmdCfgSet("width_cm", 450))
        assertEquals("PWM RUN 2000 60", SprayerProtocol.cmdPwmRun(2000, 60))
        assertEquals(115200L, SprayerProtocol.BAUD_RATES[7])
    }
}
