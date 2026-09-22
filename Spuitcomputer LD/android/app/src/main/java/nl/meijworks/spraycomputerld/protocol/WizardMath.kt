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
}
