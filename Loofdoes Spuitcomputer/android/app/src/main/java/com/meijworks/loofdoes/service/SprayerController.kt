package com.meijworks.loofdoes.service

import com.meijworks.loofdoes.SprayerState
import com.meijworks.loofdoes.audio.AlarmSound
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

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

    /** Re-read the calibration tables and settings from the board. */
    fun refresh() = service?.refresh()

    /** Send one raw protocol line; for the bench console under Advanced. */
    fun sendRaw(line: String) = service?.sendRaw(line)

    fun previewSound(sound: AlarmSound) = service?.previewSound(sound)
    fun stopAlarm() = service?.stopAlarm()
}
