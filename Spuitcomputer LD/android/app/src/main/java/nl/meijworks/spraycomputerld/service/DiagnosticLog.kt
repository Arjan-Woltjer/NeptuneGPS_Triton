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

import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

/**
 * Everything the app saw, kept for an export (NeptuneGPS_Triton#128).
 *
 * This is deliberately not the console log. The console hides the 5 Hz status
 * and 1 Hz GPS lines unless asked, because they push every reply out of view
 * within seconds; the export is evidence from a field failure, so it takes the
 * lot and the toggle does not apply to it.
 *
 * The bound is in bytes rather than lines, so it holds whatever is being
 * logged: a run with raw NMEA on produces far longer lines than one without,
 * and a line count would mean a wildly different span of time in each case. At
 * the firmware's own rates 2 MB covers roughly 70 minutes of status and GPS,
 * or 38 with raw NMEA on top.
 *
 * Nothing is written to disk until an export is asked for, so there is nothing
 * to clean up if the app is killed and nothing accumulating between exports.
 */
class DiagnosticLog(
    private val maxBytes: Int = MAX_BYTES,
    private val clock: () -> Long = System::currentTimeMillis,
    private val zone: ZoneId = ZoneId.systemDefault(),
) {
    private val lines = ArrayDeque<String>()
    private var bytes = 0
    private val lock = Any()

    /**
     * Stamp [entry] with the wall-clock time and keep it, evicting the oldest
     * lines until the buffer is back inside its bound.
     *
     * Millisecond resolution: status lines are 200 ms apart, and when chasing a
     * dropout the order of what happened within one second is the whole point.
     */
    fun append(entry: String) {
        val line = TIME.format(Instant.ofEpochMilli(clock()).atZone(zone)) + " " + entry
        synchronized(lock) {
            lines.addLast(line)
            bytes += weigh(line)
            // Never evict to empty: one line longer than the whole buffer is
            // still the only thing we have.
            while (bytes > maxBytes && lines.size > 1) {
                bytes -= weigh(lines.removeFirst())
            }
        }
    }

    fun snapshot(): List<String> = synchronized(lock) { lines.toList() }

    fun byteCount(): Int = synchronized(lock) { bytes }

    fun clear() = synchronized(lock) {
        lines.clear()
        bytes = 0
    }

    /**
     * The file's contents: [header] lines, a separator, then the buffer. A log
     * sent on its own has to say what produced it, or it is a wall of numbers.
     */
    fun render(header: List<String>): String =
        (header + SEPARATOR + snapshot()).joinToString("\n", postfix = "\n")

    /** UTF-8 bytes plus the newline it is written with. */
    private fun weigh(line: String) = line.toByteArray(Charsets.UTF_8).size + 1

    companion object {
        const val MAX_BYTES = 2 * 1024 * 1024
        const val SEPARATOR = "--"

        private val TIME: DateTimeFormatter = DateTimeFormatter.ofPattern("HH:mm:ss.SSS")

        /** The one the service writes to. */
        val shared = DiagnosticLog()
    }
}
