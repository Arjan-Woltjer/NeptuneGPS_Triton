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

import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.audio.AlarmSound
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Process-wide bridge between the UI and [SprayerService]. The service
 * registers itself here; the UI reads [state] and calls the commands.
 */
object SprayerController {
    private val _state = MutableStateFlow(SprayerState())
    val state: StateFlow<SprayerState> get() = _state

    @Volatile internal var service: SprayerService? = null

    internal fun publish(update: (SprayerState) -> SprayerState) {
        _state.value = update(_state.value)
    }

    val isRunning: Boolean get() = service != null

    // Console options. Telemetry (S:/G:) is hidden from the log by default;
    // raw NMEA (N:) is a board-side switch and always logged while on.
    private val _showTelemetry = MutableStateFlow(false)
    val showTelemetry: StateFlow<Boolean> get() = _showTelemetry.asStateFlow()
    fun setShowTelemetry(on: Boolean) { _showTelemetry.value = on }

    private val _nmeaEnabled = MutableStateFlow(false)
    val nmeaEnabled: StateFlow<Boolean> get() = _nmeaEnabled.asStateFlow()
    fun setNmea(on: Boolean) { _nmeaEnabled.value = on; service?.setNmea(on) }

    fun clearLog() = publish { it.copy(log = emptyList()) }

    /** Re-read the calibration tables and settings from the board. */
    fun refresh() = service?.refresh()

    /** Send one raw protocol line; for the bench console under Advanced. */
    fun sendRaw(line: String) = service?.sendRaw(line)

    fun previewSound(sound: AlarmSound) = service?.previewSound(sound)
    fun stopAlarm() = service?.stopAlarm()

    // Board settings (ConfigSprayer): validated on the board, re-read after.
    fun setConfig(key: String, value: Long) = service?.setConfig(key, value)

    // Pump curve point edit (serial menu option 4).
    fun editPwmPointFlow(index: Int, flowMlMin: Int) = service?.editPwmPointFlow(index, flowMlMin)

    // Calibration wizard, driven by the screens, executed in the service.
    fun startWizard(mode: WizardMode, singleIndex: Int = 0) = service?.wizard?.start(mode, singleIndex)
    fun wizardCaptureDose(rawNow: Int?) = service?.wizard?.captureDose(rawNow)
    fun wizardEnterDose(doseLha: Int) = service?.wizard?.enterDose(doseLha)
    fun wizardSetFindDuty(duty: Int) = service?.wizard?.setFindDuty(duty)
    fun wizardCaptureStart() = service?.wizard?.captureStart()
    fun wizardStartRun() = service?.wizard?.startRun()
    fun wizardStopRun() = service?.wizard?.stopRun()
    fun wizardEnterVolume(ml: Int) = service?.wizard?.enterVolume(ml)
    fun cancelWizard() = service?.wizard?.cancel()
    /** After DONE or FAILED: drop the wizard state without touching the board again. */
    fun clearWizard() = service?.clearWizard()
}
