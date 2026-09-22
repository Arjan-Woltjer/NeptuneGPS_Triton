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
