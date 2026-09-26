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

import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.content.FileProvider
import nl.meijworks.spraycomputerld.BuildConfig
import nl.meijworks.spraycomputerld.SprayerState
import java.io.File
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

/**
 * Writes the diagnostic log to a file and hands it to a share target
 * (NeptuneGPS_Triton#128). Nothing is sent anywhere by the app: the operator
 * chooses where it goes, which is why the Data safety answer stays "no data
 * collected, no data shared".
 */
object LogExport {

    private val STAMP: DateTimeFormatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")
    private val FILE_STAMP: DateTimeFormatter = DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss")

    /**
     * What produced this log. Without it a file sent on its own cannot be
     * matched to a build or a board, which is most of its value gone.
     */
    fun header(state: SprayerState, atMillis: Long, zone: ZoneId = ZoneId.systemDefault()): List<String> {
        val at = Instant.ofEpochMilli(atMillis).atZone(zone)
        return listOf(
            "MeijWorks SprayComputer LD ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE}), commit ${BuildConfig.GIT_SHA}",
            "Firmware ${state.firmwareVersion ?: "unknown"}, protocol ${state.protocolVersion ?: "unknown"}",
            "Exported ${STAMP.format(at)}, device ${Build.MANUFACTURER} ${Build.MODEL}, Android ${Build.VERSION.RELEASE}",
        )
    }

    /**
     * Write the log into the cache directory the FileProvider publishes. One
     * file per export, named by the moment it was taken.
     */
    fun write(
        context: Context,
        log: DiagnosticLog = DiagnosticLog.shared,
        state: SprayerState,
        atMillis: Long = System.currentTimeMillis(),
    ): File {
        val dir = File(context.cacheDir, DIR).apply { mkdirs() }
        // Old exports are the operator's to keep or lose; the cache is not a
        // place to accumulate them, so each export clears what came before.
        dir.listFiles()?.forEach { it.delete() }
        val stamp = FILE_STAMP.format(Instant.ofEpochMilli(atMillis).atZone(ZoneId.systemDefault()))
        return File(dir, "spraycomputer-$stamp.txt").apply {
            writeText(log.render(header(state, atMillis)))
        }
    }

    /** A chooser-ready intent for [file]. */
    fun shareIntent(context: Context, file: File, subject: String): Intent {
        val uri = FileProvider.getUriForFile(context, "${context.packageName}.logs", file)
        return Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_STREAM, uri)
            putExtra(Intent.EXTRA_SUBJECT, subject)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
    }

    const val DIR = "logs"
}
