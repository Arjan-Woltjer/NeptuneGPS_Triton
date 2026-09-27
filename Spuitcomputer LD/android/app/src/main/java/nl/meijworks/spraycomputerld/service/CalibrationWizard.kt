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

package nl.meijworks.spraycomputerld.service

import androidx.annotation.StringRes
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.UiText
import nl.meijworks.spraycomputerld.uiText
import nl.meijworks.spraycomputerld.protocol.DosePoint
import nl.meijworks.spraycomputerld.protocol.PwmPoint
import nl.meijworks.spraycomputerld.protocol.SprayerProtocol
import nl.meijworks.spraycomputerld.protocol.WizardMath
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

enum class WizardMode {
    DOSE_ONLY,   // the three knob positions
    DOSE_SINGLE, // one knob position re-entered
    PUMP_ONLY,   // the whole pump curve: find the start, a one-minute run per point
    PUMP_SINGLE, // one pump point: a one-minute run at its stored duty
    PUMP_START,  // find the start again; redo point 1, or all of them if it moved too far
}

enum class WizardStep {
    STARTING,
    DOSE_CAPTURE,     // set the knob, tap Capture
    DOSE_ENTER,       // type the l/ha for the captured reading
    PUMP_FIND,        // slider until the pump just starts flowing, tap Capture
    PUMP_STEP_READY,  // point i/5 at duty d: tap Start 1-minute run
    PUMP_RUNNING,     // counting down on the board
    PUMP_ENTER,       // type the ml collected
    REFUSED,          // a single point crossed a neighbour: nothing saved
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
    val pumpTarget: Int? = null,            // the one pump point being replaced; null for the whole curve
    val doseBefore: List<DosePoint> = emptyList(),  // the tables as they were at the start, for the
    val pumpBefore: List<PwmPoint> = emptyList(),   // neighbour check and the graph's old point
    val startMovedTooFar: Boolean = false,  // PUMP_START turned into the whole curve
    val refusedFlow: Int? = null,
    val refusedCrossing: WizardMath.Crossing? = null,
    val secondsRemaining: Int? = null,
    val busy: Boolean = false,          // a command is in flight; buttons disabled
    val message: UiText? = null,        // last error or hint
) {
    /** Resource id of the knob position being calibrated, drawn by the screen. */
    @get:StringRes
    val doseLabel: Int get() = WizardMath.DOSE_LABELS.getOrElse(doseIndex) { WizardMath.DOSE_LABELS[0] }
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

    /**
     * [doseTable] and [pumpTable] are the board's tables as the app last read
     * them: a single pump point is checked against its neighbours there, and
     * the screen draws the old point from them after a redo.
     */
    fun start(
        mode: WizardMode,
        singleIndex: Int = 0,
        doseTable: List<DosePoint> = emptyList(),
        pumpTable: List<PwmPoint> = emptyList(),
    ) {
        cancelJob()
        state = WizardState(
            mode = mode,
            doseIndex = if (mode == WizardMode.DOSE_SINGLE) singleIndex else 0,
            doseBefore = doseTable,
            pumpBefore = pumpTable,
        )
        publish(state)
        if (mode == WizardMode.PUMP_SINGLE && pumpTable.getOrNull(singleIndex) == null) {
            fail(uiText(R.string.msg_wizard_table_not_read))
            return
        }
        perform {
            when (val r = command(SprayerProtocol.cmdCalMode(true))) {
                Reply.Ok -> set {
                    when (mode) {
                        WizardMode.DOSE_ONLY, WizardMode.DOSE_SINGLE -> it.copy(step = WizardStep.DOSE_CAPTURE)
                        WizardMode.PUMP_ONLY, WizardMode.PUMP_START -> it.copy(step = WizardStep.PUMP_FIND)
                        WizardMode.PUMP_SINGLE -> it.copy(
                            step = WizardStep.PUMP_STEP_READY,
                            pwmSteps = listOf(pumpTable[singleIndex].pwm),
                            pwmIndex = 0,
                            pumpTarget = singleIndex,
                        )
                    }
                }
                Reply.Busy -> fail(uiText(R.string.msg_wizard_busy_serial))
                is Reply.Error -> fail(uiText(R.string.msg_board_refused, r.reason))
            }
        }
    }

    /** DOSE_CAPTURE: the knob is where the operator wants it; remember the reading. */
    fun captureDose(rawNow: Int?) {
        val s = state ?: return
        if (s.step != WizardStep.DOSE_CAPTURE || s.busy) return
        if (rawNow == null) { set { it.copy(message = uiText(R.string.msg_wizard_no_reading)) }; return }
        set { it.copy(capturedAnalog = rawNow, step = WizardStep.DOSE_ENTER, message = null) }
    }

    /** DOSE_ENTER: the l/ha this knob position means. */
    fun enterDose(doseLha: Int) {
        val s = state ?: return
        if (s.step != WizardStep.DOSE_ENTER || s.busy) return
        val analog = s.capturedAnalog ?: return
        if (doseLha <= 0) { set { it.copy(message = uiText(R.string.msg_wizard_positive_number)) }; return }
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
            set { it.copy(message = uiText(R.string.msg_wizard_slide_up)) }
            return
        }
        // PUMP_START redoes only point 1 unless the start moved more than
        // half the spacing to point 2 (NeptuneGPS_Triton#179). Without a
        // readable table there is nothing to keep, so it redoes the lot.
        val old = s.pumpBefore
        val onlyPointOne = s.mode == WizardMode.PUMP_START && old.size >= 2 &&
            !WizardMath.startMovedTooFar(old[0].pwm, old[1].pwm, start)
        perform {
            command(SprayerProtocol.cmdPwmSet(0))   // off until a run is started
            set {
                it.copy(
                    startPwm = start,
                    pwmSteps = if (onlyPointOne) listOf(start) else WizardMath.pwmSteps(start),
                    pwmIndex = 0,
                    pwmCaptured = emptyList(),
                    pumpTarget = if (onlyPointOne) 0 else null,
                    startMovedTooFar = s.mode == WizardMode.PUMP_START && !onlyPointOne && old.size >= 2,
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
            set {
                it.copy(
                    step = WizardStep.PUMP_STEP_READY,
                    secondsRemaining = null,
                    message = uiText(R.string.msg_wizard_run_stopped),
                )
            }
        }
    }

    /** PUMP_ENTER: ml collected in the minute equals ml/min for this duty. */
    fun enterVolume(ml: Int) {
        val s = state ?: return
        if (s.step != WizardStep.PUMP_ENTER || s.busy) return
        if (ml < 1 || ml > WizardMath.MAX_FLOW_ML_MIN) {
            set { it.copy(message = uiText(R.string.msg_wizard_volume_range, WizardMath.MAX_FLOW_ML_MIN)) }
            return
        }
        s.pumpTarget?.let { target -> saveSinglePumpPoint(s, target, ml); return }
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
            if (!ok) { fail(uiText(R.string.msg_wizard_point_refused)); return@perform }
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
            set {
                it.copy(
                    step = WizardStep.FAILED,
                    busy = false,
                    secondsRemaining = null,
                    message = uiText(R.string.msg_wizard_link_lost),
                )
            }
        }
    }

    /**
     * One pump point replaced: refused outright if it would cross a
     * neighbour, so the curve always rises (NeptuneGPS_Triton#179). CAL MODE 1
     * staged the live table, so staging this point and saving changes only it.
     */
    private fun saveSinglePumpPoint(s: WizardState, target: Int, ml: Int) {
        val crossing = WizardMath.crossing(s.pumpBefore, target, ml)
        if (crossing != null) {
            set {
                it.copy(
                    step = WizardStep.REFUSED,
                    refusedFlow = ml,
                    refusedCrossing = crossing,
                    pwmCaptured = emptyList(),
                    message = null,
                )
            }
            scope.launch { command(SprayerProtocol.cmdCalMode(false)) }   // hand calibration back; nothing staged is kept
            return
        }
        val point = PwmPoint(target, s.currentPwmDuty, ml)
        perform {
            set { it.copy(pwmCaptured = listOf(point), step = WizardStep.SAVING) }
            if (command(SprayerProtocol.cmdCalPwm(point.index, point.pwm, point.flowMlMin)) != Reply.Ok) {
                fail(uiText(R.string.msg_wizard_point_refused)); return@perform
            }
            if (save()) finish()
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
        // Bench 2026-09-13: the screens kept showing the tables read at
        // connect time (three pump points) after a five-point run had saved.
        // The service commits the C: lines this answer brings on its OK.
        command(SprayerProtocol.CMD_CAL_GET)
        set { it.copy(step = WizardStep.DONE, busy = false, message = null) }
    }

    private fun fail(message: UiText) {
        set { it.copy(step = WizardStep.FAILED, busy = false, secondsRemaining = null, message = message) }
        scope.launch { command(SprayerProtocol.cmdCalMode(false)) }
    }

    private fun failFromReply(r: Reply) = when (r) {
        Reply.Busy -> fail(uiText(R.string.msg_wizard_busy_serial_short))
        is Reply.Error -> fail(uiText(R.string.msg_board_refused, r.reason))
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
