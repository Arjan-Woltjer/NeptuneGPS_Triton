/*
  SprayComputer LD - Android companion app for the haulm sprayer computer
  Copyright (C) 2026 J.A. Woltjer.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

package nl.meijworks.spraycomputerld.audio

import androidx.annotation.StringRes
import nl.meijworks.spraycomputerld.R

import kotlin.math.PI
import kotlin.math.exp
import kotlin.math.sin

const val SAMPLE_RATE = 44_100

/**
 * The selectable alarm sounds. All except SYSTEM_ALARM are synthesised on the
 * fly so the app ships without audio assets and every sound loops cleanly.
 */
enum class AlarmSound(
    val id: String,
    @StringRes val labelRes: Int,
    @StringRes val descriptionRes: Int,
) {
    BUZZER("buzzer", R.string.sound_buzzer, R.string.sound_buzzer_desc),
    WRONG("wrong", R.string.sound_wrong, R.string.sound_wrong_desc),
    SIREN("siren", R.string.sound_siren, R.string.sound_siren_desc),
    BEEPS("beeps", R.string.sound_beeps, R.string.sound_beeps_desc),
    KLAXON("klaxon", R.string.sound_klaxon, R.string.sound_klaxon_desc),
    SYSTEM_ALARM("system", R.string.sound_system, R.string.sound_system_desc);

    val isSynthesized: Boolean get() = this != SYSTEM_ALARM

    companion object {
        fun fromId(id: String?): AlarmSound = entries.firstOrNull { it.id == id } ?: BUZZER
    }

    /** Render one loop of the sound as 16-bit mono PCM. */
    fun render(): ShortArray = when (this) {
        BUZZER -> Synth.build(0.60) { t, _ ->
            0.5 * Synth.square(t, 150.0) + 0.3 * Synth.square(t, 300.0) + 0.2 * Synth.saw(t, 152.0)
        }
        WRONG -> Synth.build(0.75) { t, len ->
            val env = if (t > len - 0.12) (len - t) / 0.12 else 1.0
            env * (0.5 * Synth.square(t, 110.0) + 0.5 * Synth.square(t, 165.0))
        }
        SIREN -> Synth.sweep(seconds = 1.20, lowHz = 600.0, highHz = 1200.0)
        BEEPS -> Synth.build(0.54) { t, _ ->
            val period = 0.18
            val inPeriod = t % period
            if (inPeriod < 0.10) 0.9 * sin(2 * PI * 1000.0 * t) * Synth.edge(inPeriod, 0.10) else 0.0
        }
        KLAXON -> Synth.build(0.80) { t, _ ->
            val am = 0.6 + 0.4 * sin(2 * PI * 9.0 * t)
            am * (0.6 * Synth.saw(t, 400.0) + 0.4 * Synth.square(t, 200.0))
        }
        SYSTEM_ALARM -> ShortArray(0)
    }
}

object Synth {
    /** Builds [seconds] of audio, generator returns a value in -1..1 for time t. */
    fun build(seconds: Double, gen: (t: Double, len: Double) -> Double): ShortArray {
        val n = (seconds * SAMPLE_RATE).toInt()
        val out = ShortArray(n)
        for (i in 0 until n) {
            val t = i.toDouble() / SAMPLE_RATE
            val v = gen(t, seconds).coerceIn(-1.0, 1.0)
            out[i] = (v * Short.MAX_VALUE * 0.95).toInt().toShort()
        }
        return out
    }

    fun square(t: Double, f: Double): Double = if ((t * f) % 1.0 < 0.5) 1.0 else -1.0
    fun saw(t: Double, f: Double): Double = 2.0 * ((t * f) % 1.0) - 1.0

    /** Short fade in/out at the ends of a beep to avoid clicks. */
    fun edge(t: Double, len: Double, ramp: Double = 0.005): Double = when {
        t < ramp -> t / ramp
        t > len - ramp -> (len - t) / ramp
        else -> 1.0
    }

    /** A phase-continuous sine sweep low -> high -> low, loops cleanly. */
    fun sweep(seconds: Double, lowHz: Double, highHz: Double): ShortArray {
        val n = (seconds * SAMPLE_RATE).toInt()
        val out = ShortArray(n)
        var phase = 0.0
        for (i in 0 until n) {
            val p = i.toDouble() / n                      // 0..1 through the loop
            val tri = 1.0 - kotlin.math.abs(2 * p - 1)    // 0 -> 1 -> 0
            val f = lowHz + (highHz - lowHz) * tri
            phase += 2 * PI * f / SAMPLE_RATE
            out[i] = (sin(phase) * Short.MAX_VALUE * 0.9).toInt().toShort()
        }
        return out
    }

    @Suppress("unused")
    fun decay(t: Double, tau: Double): Double = exp(-t / tau)
}
