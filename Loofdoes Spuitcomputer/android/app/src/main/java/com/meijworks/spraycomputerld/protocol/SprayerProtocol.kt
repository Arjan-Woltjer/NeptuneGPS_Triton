package com.meijworks.spraycomputerld.protocol

/**
 * The line protocol the Loofdoes board speaks over BLE (see RemoteSprayer.hpp
 * in the firmware, NeptuneGPS_Triton#47). Pure Kotlin so it can be unit-tested
 * without Android.
 *
 * Board -> app lines:
 *   V:<fw>,<proto>
 *   S:<speed>,<req>,<act>,<flow>,<raw>,<mixer>,<vern>,<pump>,<pumpPwm>,<dev>,<cal>,<in1..4>,<out1..4>
 *     (the last two fields came with protocol 2; a protocol 1 board omits them)
 *   N:<sentence>            a GPS sentence as received, while TELEM N is on
 *   G:<quality>,<lat>,<lon>,<fixAgeMs>
 *   C:D,<i>,<analog>,<dose>   C:P,<i>,<pwm>,<flow>
 *   K:<key>,<value>
 *   R:<secondsRemaining>
 *   OK | BUSY | ERR:<reason>
 */
sealed class BoardMessage {
    data class Version(val firmware: String, val protocol: Int) : BoardMessage()
    data class Status(val sample: StatusSample) : BoardMessage()
    data class Gps(val sample: GpsSample) : BoardMessage()
    data class DoseCalPoint(val point: DosePoint) : BoardMessage()
    data class PwmCalPoint(val point: PwmPoint) : BoardMessage()
    data class ConfigValue(val key: String, val value: Long) : BoardMessage()
    data class RunCountdown(val secondsRemaining: Int) : BoardMessage()
    data class Nmea(val sentence: String) : BoardMessage()
    object Ok : BoardMessage()
    object Busy : BoardMessage()
    data class Error(val reason: String) : BoardMessage()
    data class Unknown(val line: String) : BoardMessage()
}

/** One `S:` line. `actualLha` is null when the board reports it undefined (-1). */
data class StatusSample(
    val speedMs: Float,
    val requestedLha: Float,
    val actualLha: Float?,
    val flowMlMin: Float,
    val raw: Int,
    val mixer: Boolean,
    val vernevelaar: Boolean,
    val pump: Boolean,
    val pumpPwm: Int,
    val deviation: Boolean,
    val calibrationOwner: CalibrationOwner,
    val inputs: List<Boolean> = emptyList(),    // IN1..IN4, empty from a protocol 1 board
    val outputs: List<Boolean> = emptyList(),   // OUT1..OUT4
) {
    val speedKmh: Float get() = speedMs * 3.6f
}

enum class CalibrationOwner { NONE, SERIAL, APP;
    companion object {
        fun fromCode(code: Int): CalibrationOwner = when (code) {
            1 -> SERIAL
            2 -> APP
            else -> NONE
        }
    }
}

/** One `G:` line. `fixAgeMs` is null until the board has seen a position fix. */
data class GpsSample(val quality: Int, val latitude: Double, val longitude: Double, val fixAgeMs: Long?) {
    val qualityLabel: String get() = qualityLabel(quality)

    /** The parser reports 999999.9 until the first position; anything outside the globe is "none". */
    val hasPosition: Boolean get() = latitude in -90.0..90.0 && longitude in -180.0..180.0

    companion object {
        fun qualityLabel(q: Int): String = when (q) {
            0 -> "No fix"
            1 -> "GPS"
            2 -> "DGPS"
            4 -> "RTK fixed"
            5 -> "RTK float"
            else -> "Quality $q"
        }
    }
}

data class DosePoint(val index: Int, val analog: Int, val doseLha: Int)
data class PwmPoint(val index: Int, val pwm: Int, val flowMlMin: Int)

object SprayerProtocol {
    const val PROTOCOL_VERSION = 2

    // Settings keys, as the board names them (ConfigSprayer).
    const val KEY_WIDTH_CM = "width_cm"
    const val KEY_GUIDANCE_MS = "guid_ms"
    const val KEY_GPS_BAUD = "gps_baud"
    const val KEY_GPS_MIN_QUALITY = "gps_minq"
    const val KEY_BUZZER = "buzzer"

    /** The 4800 x n table ConfigSprayer uses for `gps_baud`. */
    val BAUD_RATES: List<Long> = listOf(1, 2, 3, 4, 6, 8, 12, 24).map { it * 4800L }

    fun parse(rawLine: String): BoardMessage {
        val line = rawLine.trim()
        if (line.isEmpty()) return BoardMessage.Unknown(rawLine)
        if (line == "OK") return BoardMessage.Ok
        if (line == "BUSY") return BoardMessage.Busy
        if (line.startsWith("ERR:")) return BoardMessage.Error(line.substring(4))

        if (line.length < 2 || line[1] != ':') return BoardMessage.Unknown(line)
        val body = line.substring(2)
        val f = body.split(',')
        return try {
            when (line[0]) {
                'V' -> if (f.size >= 2) BoardMessage.Version(f[0], f[1].toInt()) else BoardMessage.Unknown(line)
                'S' -> parseStatus(f) ?: BoardMessage.Unknown(line)
                'G' -> if (f.size >= 4) {
                    val age = f[3].toLong()
                    BoardMessage.Gps(GpsSample(f[0].toInt(), f[1].toDouble(), f[2].toDouble(), if (age < 0) null else age))
                } else BoardMessage.Unknown(line)
                'C' -> when {
                    f.size >= 4 && f[0] == "D" -> BoardMessage.DoseCalPoint(DosePoint(f[1].toInt(), f[2].toInt(), f[3].toInt()))
                    f.size >= 4 && f[0] == "P" -> BoardMessage.PwmCalPoint(PwmPoint(f[1].toInt(), f[2].toInt(), f[3].toInt()))
                    else -> BoardMessage.Unknown(line)
                }
                'K' -> if (f.size >= 2) BoardMessage.ConfigValue(f[0], f[1].toLong()) else BoardMessage.Unknown(line)
                'R' -> BoardMessage.RunCountdown(f[0].toInt())
                'N' -> BoardMessage.Nmea(body)
                else -> BoardMessage.Unknown(line)
            }
        } catch (e: NumberFormatException) {
            BoardMessage.Unknown(line)
        }
    }

    private fun parseStatus(f: List<String>): BoardMessage? {
        if (f.size < 11) return null
        val actual = f[2].toFloat()
        return BoardMessage.Status(
            StatusSample(
                speedMs = f[0].toFloat(),
                requestedLha = f[1].toFloat(),
                actualLha = if (actual < 0f) null else actual,
                flowMlMin = f[3].toFloat(),
                raw = f[4].toInt(),
                mixer = f[5] == "1",
                vernevelaar = f[6] == "1",
                pump = f[7] == "1",
                pumpPwm = f[8].toInt(),
                deviation = f[9] == "1",
                calibrationOwner = CalibrationOwner.fromCode(f[10].toInt()),
                inputs = if (f.size >= 13) bits(f[11]) else emptyList(),
                outputs = if (f.size >= 13) bits(f[12]) else emptyList(),
            )
        )
    }

    /** "1001" -> [true, false, false, true]; anything but 0/1 is unknown, hence false. */
    private fun bits(field: String): List<Boolean> = field.trim().map { it == '1' }

    // Commands (app -> board). Each is one line; the client appends the newline.
    const val CMD_PING = "PING"
    const val CMD_INFO = "INFO"
    const val CMD_CAL_GET = "CAL GET"
    const val CMD_CFG_GET = "CFG GET"
    fun cmdTelemetryStatus(on: Boolean) = "TELEM S ${if (on) 1 else 0}"
    fun cmdTelemetryGps(on: Boolean) = "TELEM G ${if (on) 1 else 0}"
    fun cmdTelemetryNmea(on: Boolean) = "TELEM N ${if (on) 1 else 0}"
    fun cmdCalMode(on: Boolean) = "CAL MODE ${if (on) 1 else 0}"
    fun cmdCalDose(index: Int, analog: Int, doseLha: Int) = "CAL DOSE $index $analog $doseLha"
    fun cmdCalPwm(index: Int, pwm: Int, flowMlMin: Int) = "CAL PWM $index $pwm $flowMlMin"
    fun cmdCalPwmCount(n: Int) = "CAL PWMN $n"
    const val CMD_CAL_SAVE = "CAL SAVE"
    fun cmdPwmSet(duty: Int) = "PWM SET $duty"
    fun cmdCfgSet(key: String, value: Long) = "CFG SET $key $value"
    fun cmdPwmRun(duty: Int, seconds: Int) = "PWM RUN $duty $seconds"
    const val CMD_PWM_STOP = "PWM STOP"

    /**
     * Commands that move an output or persist go to the board's secure
     * characteristic (bonded, authenticated); the rest to the open one.
     * Mirrors RemoteSprayer::IsProtected() on the board.
     */
    fun isProtected(line: String): Boolean {
        val t = line.trim().split(Regex("\\s+"))
        return when (t.getOrNull(0)) {
            "PWM" -> true
            "CAL" -> t.getOrNull(1) != "GET"
            "CFG" -> t.getOrNull(1) == "SET"
            else -> false
        }
    }
}
