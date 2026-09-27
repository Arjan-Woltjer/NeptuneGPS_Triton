package nl.meijworks.spraycomputerld.service

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import nl.meijworks.spraycomputerld.protocol.PwmPoint
import nl.meijworks.spraycomputerld.protocol.WizardMath
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * The pump paths of Pompkalibratie (NeptuneGPS_Triton#179), against a fake
 * board that answers OK to everything and records what it was sent.
 * Dispatchers.Unconfined runs every launch to completion on the spot, so each
 * call leaves the wizard in its next state before the assertions look.
 */
class CalibrationWizardTest {

    private val sent = mutableListOf<String>()
    private val wizard = CalibrationWizard(
        scope = CoroutineScope(Dispatchers.Unconfined),
        command = { line -> sent += line; Reply.Ok },
        publish = {},
    )
    private val step get() = wizard.state?.step

    /** The table from the 2026-09-26 bench board. Spacing 1338 -> 2027 is 689. */
    private val table = listOf(
        PwmPoint(0, 1338, 200), PwmPoint(1, 2027, 600), PwmPoint(2, 2716, 1100),
        PwmPoint(3, 3450, 1450), PwmPoint(4, 4095, 1800),
    )

    private fun runOnePoint(ml: Int) {
        wizard.startRun()
        wizard.onCountdown(0)                 // the board ended the run
        assertEquals(WizardStep.PUMP_ENTER, step)
        wizard.enterVolume(ml)
    }

    // ------------------------------------------------------------ one point

    @Test
    fun singlePoint_runsAtItsStoredDuty_andSavesOnlyThatPoint() {
        wizard.start(WizardMode.PUMP_SINGLE, 2, pumpTable = table)
        assertEquals(WizardStep.PUMP_STEP_READY, step)
        assertEquals(2716, wizard.state?.currentPwmDuty)

        runOnePoint(1000)

        assertEquals(WizardStep.DONE, step)
        assertTrue(sent.contains("PWM RUN 2716 60"))
        assertTrue(sent.contains("CAL PWM 2 2716 1000"))
        assertTrue(sent.contains("CAL SAVE"))
        // The whole-curve path resets the count; one point must not.
        assertFalse(sent.any { it.startsWith("CAL PWMN") })
        assertEquals(1, sent.count { it.startsWith("CAL PWM ") })
    }

    @Test
    fun singlePoint_belowItsPreviousNeighbour_isRefused_andNothingIsSaved() {
        wizard.start(WizardMode.PUMP_SINGLE, 2, pumpTable = table)

        runOnePoint(600)                      // equal to point 2's flow: not strictly above

        assertEquals(WizardStep.REFUSED, step)
        assertEquals(600, wizard.state?.refusedFlow)
        assertEquals(WizardMath.Crossing.BELOW_PREVIOUS, wizard.state?.refusedCrossing)
        assertFalse(sent.any { it.startsWith("CAL PWM ") })
        assertFalse(sent.contains("CAL SAVE"))
        assertEquals("CAL MODE 0", sent.last())   // calibration handed back
    }

    @Test
    fun singlePoint_aboveItsNextNeighbour_isRefused() {
        wizard.start(WizardMode.PUMP_SINGLE, 2, pumpTable = table)
        runOnePoint(1500)
        assertEquals(WizardStep.REFUSED, step)
        assertEquals(WizardMath.Crossing.ABOVE_NEXT, wizard.state?.refusedCrossing)
        assertFalse(sent.contains("CAL SAVE"))
    }

    @Test
    fun singlePoint_withoutATable_failsBeforeTouchingTheBoard() {
        wizard.start(WizardMode.PUMP_SINGLE, 2, pumpTable = emptyList())
        assertEquals(WizardStep.FAILED, step)
        assertFalse(sent.any { it.startsWith("CAL MODE 1") || it.startsWith("PWM") })
    }

    // ------------------------------------------------------------ start point

    @Test
    fun startPoint_smallMove_redoesPointOneOnly_atTheNewDuty() {
        wizard.start(WizardMode.PUMP_START, pumpTable = table)
        assertEquals(WizardStep.PUMP_FIND, step)
        wizard.setFindDuty(1600)              // moved 262; half the spacing is 344
        wizard.captureStart()

        assertEquals(listOf(1600), wizard.state?.pwmSteps)
        assertEquals(0, wizard.state?.pumpTarget)
        assertFalse(wizard.state!!.startMovedTooFar)

        runOnePoint(150)

        assertEquals(WizardStep.DONE, step)
        assertTrue(sent.contains("CAL PWM 0 1600 150"))
        assertEquals(1, sent.count { it.startsWith("CAL PWM ") })
    }

    @Test
    fun startPoint_largeMove_redoesTheWholeCurve() {
        wizard.start(WizardMode.PUMP_START, pumpTable = table)
        wizard.setFindDuty(1700)              // moved 362 > 344
        wizard.captureStart()

        assertEquals(WizardMath.pwmSteps(1700), wizard.state?.pwmSteps)
        assertEquals(null, wizard.state?.pumpTarget)
        assertTrue(wizard.state!!.startMovedTooFar)
    }

    @Test
    fun startPoint_pointOneThatCrossesPointTwo_isRefused() {
        wizard.start(WizardMode.PUMP_START, pumpTable = table)
        wizard.setFindDuty(1400)
        wizard.captureStart()
        runOnePoint(650)                      // above point 2's 600
        assertEquals(WizardStep.REFUSED, step)
        assertEquals(WizardMath.Crossing.ABOVE_NEXT, wizard.state?.refusedCrossing)
        assertFalse(sent.contains("CAL SAVE"))
    }

    @Test
    fun startPoint_withoutATable_redoesTheWholeCurve() {
        wizard.start(WizardMode.PUMP_START, pumpTable = emptyList())
        wizard.setFindDuty(1400)
        wizard.captureStart()
        assertEquals(WizardMath.PWM_STEPS, wizard.state?.pwmSteps?.size)
        assertEquals(null, wizard.state?.pumpTarget)
    }
}
