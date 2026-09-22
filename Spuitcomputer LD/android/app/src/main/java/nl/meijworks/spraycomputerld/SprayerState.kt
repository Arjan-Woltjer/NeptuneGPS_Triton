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

import nl.meijworks.spraycomputerld.protocol.DosePoint
import nl.meijworks.spraycomputerld.protocol.GpsSample
import nl.meijworks.spraycomputerld.protocol.PwmPoint
import nl.meijworks.spraycomputerld.protocol.StatusSample
import nl.meijworks.spraycomputerld.service.WizardState

enum class ConnectionState { OFF, SCANNING, CONNECTING, CONNECTED }

/**
 * Everything the UI shows, published by the service as a StateFlow. The
 * board is the source of truth for every number here; the app only displays.
 */
data class SprayerState(
    val connection: ConnectionState = ConnectionState.OFF,
    val deviceName: String? = null,
    val firmwareVersion: String? = null,
    val protocolVersion: Int? = null,
    val status: StatusSample? = null,
    val statusAt: Long = 0,                     // SystemClock.elapsedRealtime() of the last S: line
    val gps: GpsSample? = null,
    val dosePoints: List<DosePoint> = emptyList(),
    val pwmPoints: List<PwmPoint> = emptyList(),
    val config: Map<String, Long> = emptyMap(),
    val runSecondsRemaining: Int? = null,       // set while a PWM RUN is counting down
    val wizard: WizardState? = null,            // the calibration in progress, if any
    val alarmActive: Boolean = false,
    val pairing: Boolean = false,               // Android is asking for the board's code
    val lastMessage: UiText? = null,
    val log: List<String> = emptyList(),        // last lines in both directions, for the bench
) {
    val connected: Boolean get() = connection == ConnectionState.CONNECTED

    /** True when connected but no status line has arrived for a while. */
    fun statusStale(now: Long): Boolean = connected && (status == null || now - statusAt > STATUS_STALE_MS)

    companion object {
        const val STATUS_STALE_MS = 2000L
        const val LOG_LINES = 300
    }
}
