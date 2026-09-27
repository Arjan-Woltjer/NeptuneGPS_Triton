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

package nl.meijworks.spraycomputerld.protocol

import androidx.annotation.StringRes
import nl.meijworks.spraycomputerld.R

/**
 * The numbers the serial wizard (CalibrationSprayer.cpp) uses, kept here so
 * the app's wizard produces the same points and can be unit-tested.
 */
object WizardMath {
    const val DOSE_POINTS = 3
    const val PWM_STEPS = 5
    const val MAX_DUTY = 4095
    const val RUN_SECONDS = 60
    const val MAX_FLOW_ML_MIN = 4000
    const val MIN_START_DUTY = 1      // a start point of 0 means the pump is not flowing

    /**
     * The three knob positions, named as the serial wizard names them so the
     * calibration procedure document still reads across. Resource ids, not
     * text: the operator sees them in their own language.
     */
    @get:StringRes
    val DOSE_LABELS: List<Int> = listOf(
        R.string.wizard_dose_label_minimum,
        R.string.wizard_dose_label_middle,
        R.string.wizard_dose_label_maximum,
    )

    /**
     * PWM_STEPS equally spaced duties from `startPwm` up to and including
     * MAX_DUTY, exactly as CalibrationSprayer::handlePwmFind() generates them:
     * start + (MAX - start) * i / (steps - 1), integer arithmetic.
     */
    fun pwmSteps(startPwm: Int): List<Int> {
        val start = startPwm.coerceIn(0, MAX_DUTY)
        return (0 until PWM_STEPS).map { i ->
            start + ((MAX_DUTY - start).toLong() * i / (PWM_STEPS - 1)).toInt()
        }
    }

    /** Which neighbour a re-measured pump point would cross (NeptuneGPS_Triton#179). */
    enum class Crossing { BELOW_PREVIOUS, ABOVE_NEXT }

    /**
     * Redoing the start point (point 1) redoes the other points too when it
     * moved more than half the spacing between point 1 and point 2. The other
     * duties were spaced evenly from the old start, so past that they no
     * longer belong to it. A start at or beyond point 2 is always too far: the
     * table would be out of order. Compared doubled, so no rounding.
     */
    fun startMovedTooFar(oldStart: Int, oldSecond: Int, newStart: Int): Boolean =
        newStart >= oldSecond || 2 * kotlin.math.abs(newStart - oldStart) > oldSecond - oldStart

    /**
     * A re-measured pump point must lie strictly between its neighbours'
     * flows; otherwise it is refused (NeptuneGPS_Triton#179). Null means it
     * fits. Equal flows count as crossing: the curve has to rise.
     */
    fun crossing(table: List<PwmPoint>, index: Int, flowMlMin: Int): Crossing? {
        val previous = table.getOrNull(index - 1)
        val next = table.getOrNull(index + 1)
        return when {
            previous != null && flowMlMin <= previous.flowMlMin -> Crossing.BELOW_PREVIOUS
            next != null && flowMlMin >= next.flowMlMin -> Crossing.ABOVE_NEXT
            else -> null
        }
    }
}
