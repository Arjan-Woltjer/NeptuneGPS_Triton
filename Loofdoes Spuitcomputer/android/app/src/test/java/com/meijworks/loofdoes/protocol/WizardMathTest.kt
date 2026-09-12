package com.meijworks.loofdoes.protocol

import org.junit.Assert.assertEquals
import org.junit.Test

class WizardMathTest {

    @Test
    fun pwmSteps_matchSerialWizardFormula() {
        // start + (4095 - start) * i / 4, integer division, as in handlePwmFind()
        assertEquals(listOf(0, 1023, 2047, 3071, 4095), WizardMath.pwmSteps(0))
        assertEquals(listOf(1000, 1773, 2547, 3321, 4095), WizardMath.pwmSteps(1000))
        assertEquals(listOf(4095, 4095, 4095, 4095, 4095), WizardMath.pwmSteps(4095))
    }

    @Test
    fun pwmSteps_alwaysEndAtFullDuty_andClampInput() {
        assertEquals(4095, WizardMath.pwmSteps(123).last())
        assertEquals(WizardMath.pwmSteps(0), WizardMath.pwmSteps(-50))
        assertEquals(WizardMath.pwmSteps(4095), WizardMath.pwmSteps(9999))
    }

    @Test
    fun commandsForTheWizard() {
        assertEquals("CAL DOSE 1 2048 100", SprayerProtocol.cmdCalDose(1, 2048, 100))
        assertEquals("CAL PWM 4 4095 3900", SprayerProtocol.cmdCalPwm(4, 4095, 3900))
        assertEquals("CAL PWMN 5", SprayerProtocol.cmdCalPwmCount(5))
        assertEquals("PWM SET 777", SprayerProtocol.cmdPwmSet(777))
        assertEquals("CAL SAVE", SprayerProtocol.CMD_CAL_SAVE)
    }
}
