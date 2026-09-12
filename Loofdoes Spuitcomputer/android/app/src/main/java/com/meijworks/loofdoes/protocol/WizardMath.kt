package com.meijworks.loofdoes.protocol

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

    val DOSE_LABELS = listOf("MINIMUM", "MIDDLE", "MAXIMUM")

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
