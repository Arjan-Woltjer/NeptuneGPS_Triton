package com.meijworks.loofdoes.service

import com.meijworks.loofdoes.protocol.DosePoint
import com.meijworks.loofdoes.protocol.PwmPoint
import com.meijworks.loofdoes.protocol.SprayerProtocol
import com.meijworks.loofdoes.protocol.WizardMath
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

enum class WizardMode {
    FULL,        // knob positions, then the pump curve: the serial wizard, step for step
    DOSE_ONLY,   // the three knob positions
    DOSE_SINGLE, // one knob position re-entered
    PUMP_ONLY,   // the pump curve
}

enum class WizardStep {
    STARTING,
    DOSE_CAPTURE,     // set the knob, tap Capture
    DOSE_ENTER,       // type the l/ha for the captured reading
    PUMP_FIND,        // slider until the pump just starts flowing, tap Capture
    PUMP_STEP_READY,  // point i/5 at duty d: tap Start 1-minute run
    PUMP_RUNNING,     // counting down on the board
    PUMP_ENTER,       // type the ml collected
    SAVING,
    DONE,
    FAILED,
}

data class WizardState(
    val mode: WizardMode,
    val step: WizardStep = WizardStep.STARTING,
    val doseIndex: Int = 0,
    val doseCaptured: List<DosePoint> = emptyList(),
    val capturedAnalog: Int? = null,
    val findDuty: Int = 0,
    val startPwm: Int? = null,
    val pwmSteps: List<Int> = emptyList(),
    val pwmIndex: Int = 0,
    val pwmCaptured: List<PwmPoint> = emptyList(),
    val secondsRemaining: Int? = null,
    val busy: Boolean = false,          // a command is in flight; buttons disabled
    val message: String? = null,        // last error or hint
) {
    val doseLabel: String get() = WizardMath.DOSE_LABELS.getOrElse(doseIndex) { "" }
    val currentPwmDuty: Int get() = pwmSteps.getOrElse(pwmIndex) { 0 }
}

/**
 * The calibration procedure, driven from the app instead of the serial menu
 * (NeptuneGPS_Triton#52). Lives in the service so a 60 s pump run keeps
 * going and the wizard moves on even with the screen asleep. Every step that
 * touches the board awaits its reply; the pump run itself is timed and ended
 * by the board, this class only follows the R: countdown.
 *
 * Edits are staged on the board (CAL DOSE / CAL PWM) and only applied by
 * CAL SAVE, so cancelling or losing the link leaves the live tables alone.
 */
class CalibrationWizard(
    private val scope: CoroutineScope,
    private val command: suspend (String) -> Reply,
    private val publish: (WizardState?) -> Unit,
) {
    var state: WizardState? = null
        private set

    private var job: Job? = null
    private var pendingFindDuty: Int? = null
    private var findJob: Job? = null

    private fun set(update: (WizardState) -> WizardState) {
        val s = state ?: return
        state = update(s)
        publish(state)
    }

    fun start(mode: WizardMode, singleIndex: Int = 0) {
        cancelJob()
        state = WizardState(mode = mode, doseIndex = if (mode == WizardMode.DOSE_SINGLE) singleIndex else 0)
        publish(state)
        perform {
            when (val r = command(SprayerProtocol.cmdCalMode(true))) {
                Reply.Ok -> set {
                    it.copy(step = if (mode == WizardMode.PUMP_ONLY) WizardStep.PUMP_FIND else WizardStep.DOSE_CAPTURE)
                }
                Reply.Busy -> fail("The serial wizard on the board holds calibration. Finish it there first.")
                is Reply.Error -> fail("Board refused: ${r.reason}")
            }
        }
    }

    /** DOSE_CAPTURE: the knob is where the operator wants it; remember the reading. */
    fun captureDose(rawNow: Int?) {
        val s = state ?: return
        if (s.step != WizardStep.DOSE_CAPTURE || s.busy) return
        if (rawNow == null) { set { it.copy(message = "No reading from the board yet") }; return }
        set { it.copy(capturedAnalog = rawNow, step = WizardStep.DOSE_ENTER, message = null) }
    }

    /** DOSE_ENTER: the l/ha this knob position means. */
    fun enterDose(doseLha: Int) {
        val s = state ?: return
        if (s.step != WizardStep.DOSE_ENTER || s.busy) return
        val analog = s.capturedAnalog ?: return
        if (doseLha <= 0) { set { it.copy(message = "Enter a positive whole number") }; return }
        perform {
            when (val r = command(SprayerProtocol.cmdCalDose(s.doseIndex, analog, doseLha))) {
                Reply.Ok -> {
                    val captured = s.doseCaptured + DosePoint(s.doseIndex, analog, doseLha)
                    val next = s.doseIndex + 1
                    val doseDone = s.mode == WizardMode.DOSE_SINGLE || next >= WizardMath.DOSE_POINTS
                    when {
                        !doseDone -> set {
                            it.copy(doseCaptured = captured, doseIndex = next, capturedAnalog = null,
                                step = WizardStep.DOSE_CAPTURE, message = null)
                        }
                        s.mode == WizardMode.FULL -> {
                            // Save the knob half now, so a cancelled pump half keeps it.
                            set { it.copy(doseCaptured = captured, step = WizardStep.SAVING) }
                            if (save()) set { it.copy(step = WizardStep.PUMP_FIND, findDuty = 0, message = null) }
                        }
                        else -> {
                            set { it.copy(doseCaptured = captured, step = WizardStep.SAVING) }
                            if (save()) finish()
                        }
                    }
                }
                else -> failFromReply(r)
            }
        }
    }

    /** PUMP_FIND: slider moved; the board follows with PWM SET, rate-limited. */
    fun setFindDuty(duty: Int) {
        val s = state ?: return
        if (s.step != WizardStep.PUMP_FIND) return
        val d = duty.coerceIn(0, WizardMath.MAX_DUTY)
        set { it.copy(findDuty = d) }
        pendingFindDuty = d
        if (findJob?.isActive == true) return
        findJob = scope.launch {
            while (true) {
                val next = pendingFindDuty ?: break
                pendingFindDuty = null
                command(SprayerProtocol.cmdPwmSet(next))
                delay(FIND_RATE_MS)
                if (pendingFindDuty == null) break
            }
        }
    }

    /** PUMP_FIND: the pump just started flowing at the current slider duty. */
    fun captureStart() {
        val s = state ?: return
        if (s.step != WizardStep.PUMP_FIND || s.busy) return
        val start = s.findDuty
        // Bench 2026-09-13: a start point of 0 was captured, so the first of
        // the five runs happened at duty 0 and the pump did nothing. The pump
        // cannot be "just flowing" at zero duty.
        if (start < WizardMath.MIN_START_DUTY) {
            set { it.copy(message = "Slide up until the pump actually starts flowing before capturing") }
            return
        }
        perform {
            command(SprayerProtocol.cmdPwmSet(0))   // off until a run is started
            set {
                it.copy(
                    startPwm = start,
                    pwmSteps = WizardMath.pwmSteps(start),
                    pwmIndex = 0,
                    pwmCaptured = emptyList(),
                    step = WizardStep.PUMP_STEP_READY,
                    message = null,
                )
            }
        }
    }

    /** PUMP_STEP_READY: start the board-timed one-minute run for this point. */
    fun startRun() {
        val s = state ?: return
        if (s.step != WizardStep.PUMP_STEP_READY || s.busy) return
        perform {
            when (val r = command(SprayerProtocol.cmdPwmRun(s.currentPwmDuty, WizardMath.RUN_SECONDS))) {
                Reply.Ok -> set { it.copy(step = WizardStep.PUMP_RUNNING, secondsRemaining = WizardMath.RUN_SECONDS, message = null) }
                else -> failFromReply(r)
            }
        }
    }

    /** From the board's R: lines. R:0 means the board has ended the run. */
    fun onCountdown(secondsRemaining: Int) {
        val s = state ?: return
        if (s.step != WizardStep.PUMP_RUNNING) return
        if (secondsRemaining > 0) {
            set { it.copy(secondsRemaining = secondsRemaining) }
        } else {
            set { it.copy(secondsRemaining = null, step = WizardStep.PUMP_ENTER) }
        }
    }

    /** PUMP_RUNNING: abort this point's run; the point can be started again. */
    fun stopRun() {
        val s = state ?: return
        if (s.step != WizardStep.PUMP_RUNNING) return
        perform {
            command(SprayerProtocol.CMD_PWM_STOP)
            set { it.copy(step = WizardStep.PUMP_STEP_READY, secondsRemaining = null, message = "Run stopped, start it again") }
        }
    }

    /** PUMP_ENTER: ml collected in the minute equals ml/min for this duty. */
    fun enterVolume(ml: Int) {
        val s = state ?: return
        if (s.step != WizardStep.PUMP_ENTER || s.busy) return
        if (ml < 1 || ml > WizardMath.MAX_FLOW_ML_MIN) {
            set { it.copy(message = "Enter a whole number between 1 and ${WizardMath.MAX_FLOW_ML_MIN}") }
            return
        }
        val captured = s.pwmCaptured + PwmPoint(s.pwmIndex, s.currentPwmDuty, ml)
        val next = s.pwmIndex + 1
        if (next < s.pwmSteps.size) {
            set { it.copy(pwmCaptured = captured, pwmIndex = next, step = WizardStep.PUMP_STEP_READY, message = null) }
            return
        }
        perform {
            set { it.copy(pwmCaptured = captured, step = WizardStep.SAVING) }
            var ok = command(SprayerProtocol.cmdCalPwmCount(captured.size)) == Reply.Ok
            for (p in captured) {
                if (!ok) break
                ok = command(SprayerProtocol.cmdCalPwm(p.index, p.pwm, p.flowMlMin)) == Reply.Ok
            }
            if (!ok) { fail("Board refused a pump point"); return@perform }
            if (save()) finish()
        }
    }

    /** Leave the wizard; the board gets calibration back and discards staged edits. */
    fun cancel() {
        cancelJob()
        findJob?.cancel()
        val had = state != null
        state = null
        publish(null)
        if (had) scope.launch {
            command(SprayerProtocol.CMD_PWM_STOP)
            command(SprayerProtocol.cmdCalMode(false))
        }
    }

    /** Forget the wizard without talking to the board (after DONE or FAILED). */
    fun cancelQuietly() {
        cancelJob()
        findJob?.cancel()
        state = null
    }

    /** The link is gone: the board has already taken calibration back. */
    fun onDisconnected() {
        cancelJob()
        findJob?.cancel()
        if (state != null && state?.step != WizardStep.DONE) {
            set { it.copy(step = WizardStep.FAILED, busy = false, secondsRemaining = null, message = "Link lost; the board stopped the pump and kept its old calibration") }
        }
    }

    // ------------------------------------------------------------ helpers

    private suspend fun save(): Boolean {
        return when (val r = command(SprayerProtocol.CMD_CAL_SAVE)) {
            Reply.Ok -> true
            else -> { failFromReply(r); false }
        }
    }

    private suspend fun finish() {
        command(SprayerProtocol.cmdCalMode(false))
        set { it.copy(step = WizardStep.DONE, busy = false, message = null) }
    }

    private fun fail(message: String) {
        set { it.copy(step = WizardStep.FAILED, busy = false, secondsRemaining = null, message = message) }
        scope.launch { command(SprayerProtocol.cmdCalMode(false)) }
    }

    private fun failFromReply(r: Reply) = when (r) {
        Reply.Busy -> fail("The serial wizard on the board holds calibration")
        is Reply.Error -> fail("Board refused: ${r.reason}")
        Reply.Ok -> Unit
    }

    /** Run one board interaction with the buttons disabled meanwhile. */
    private fun perform(block: suspend () -> Unit) {
        set { it.copy(busy = true) }
        job = scope.launch {
            try {
                block()
            } finally {
                if (state != null) set { it.copy(busy = false) }
            }
        }
    }

    private fun cancelJob() {
        job?.cancel()
        job = null
    }

    companion object {
        private const val FIND_RATE_MS = 150L
    }
}
