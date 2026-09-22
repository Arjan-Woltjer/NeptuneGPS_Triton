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

package nl.meijworks.spraycomputerld

import android.content.Context
import android.content.SharedPreferences
import nl.meijworks.spraycomputerld.audio.AlarmSound
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

data class SettingsState(
    val alarmEnabled: Boolean = true,
    val sound: AlarmSound = AlarmSound.BUZZER,
    val volume: Float = 1.0f,
    val vibrate: Boolean = true,
    val keepScreenOn: Boolean = true,
    /** False until the operator has been through the first-run explanation. */
    val onboardingDone: Boolean = false,
    /** Reveals the console and the raw calibration edits. Off in the field. */
    val developerMode: Boolean = false,
)

/**
 * Phone-side preferences only (how the alarm sounds). Everything about the
 * sprayer itself lives on the board and is edited through the protocol.
 */
object Settings {
    private const val PREFS = "spraycomputer_settings"
    private lateinit var prefs: SharedPreferences
    private val _state = MutableStateFlow(SettingsState())
    val state: StateFlow<SettingsState> get() = _state

    fun init(context: Context) {
        if (::prefs.isInitialized) return
        prefs = context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        _state.value = SettingsState(
            alarmEnabled = prefs.getBoolean("alarmEnabled", true),
            sound = AlarmSound.fromId(prefs.getString("sound", null)),
            volume = prefs.getFloat("volume", 1.0f),
            vibrate = prefs.getBoolean("vibrate", true),
            keepScreenOn = prefs.getBoolean("keepScreenOn", true),
            onboardingDone = prefs.getBoolean("onboardingDone", false),
            developerMode = prefs.getBoolean("developerMode", false),
        )
    }

    fun setAlarmEnabled(v: Boolean) = update(_state.value.copy(alarmEnabled = v)) { putBoolean("alarmEnabled", v) }
    fun setSound(sound: AlarmSound) = update(_state.value.copy(sound = sound)) { putString("sound", sound.id) }
    fun setVolume(v: Float) = update(_state.value.copy(volume = v.coerceIn(0f, 1f))) { putFloat("volume", v.coerceIn(0f, 1f)) }
    fun setVibrate(v: Boolean) = update(_state.value.copy(vibrate = v)) { putBoolean("vibrate", v) }
    fun setKeepScreenOn(v: Boolean) = update(_state.value.copy(keepScreenOn = v)) { putBoolean("keepScreenOn", v) }
    fun setOnboardingDone(v: Boolean) = update(_state.value.copy(onboardingDone = v)) { putBoolean("onboardingDone", v) }
    fun setDeveloperMode(v: Boolean) = update(_state.value.copy(developerMode = v)) { putBoolean("developerMode", v) }

    private inline fun update(newState: SettingsState, edit: SharedPreferences.Editor.() -> Unit) {
        _state.value = newState
        prefs.edit().apply(edit).apply()
    }
}
