package nl.meijworks.spraycomputerld.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.protocol.GpsSample
import nl.meijworks.spraycomputerld.protocol.SprayerProtocol
import nl.meijworks.spraycomputerld.service.SprayerController

/**
 * Receiver settings on the board plus a live readout so the effect of a
 * change is visible at once, the baudrate included.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GpsConfigScreen(sprayer: SprayerState, onBack: () -> Unit) {
    val baudIndex = sprayer.config[SprayerProtocol.KEY_GPS_BAUD]
    val minQuality = sprayer.config[SprayerProtocol.KEY_GPS_MIN_QUALITY]
    val g = sprayer.gps
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("GPS config") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("Live", style = MaterialTheme.typography.titleMedium)
                    DetailRow("Fix", g?.qualityLabel ?: "–")
                    DetailRow(
                        "Fix age",
                        when {
                            g == null -> "–"
                            g.fixAgeMs == null -> "no fix yet"
                            else -> "%.1f s".format(g.fixAgeMs / 1000.0)
                        },
                    )
                    DetailRow("Speed", sprayer.status?.let { "%.1f km/h".format(it.speedKmh) } ?: "–")
                    DetailRow("Position", g?.takeIf { it.hasPosition }?.let { "%.6f, %.6f".format(it.latitude, it.longitude) } ?: "–")
                }
            }

            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(vertical = 8.dp)) {
                    Text(
                        "Minimum fix to dose",
                        style = MaterialTheme.typography.titleMedium,
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    )
                    Text(
                        "Below this the pump stops, like it does without a speed. RTK fixed accepts only a fixed solution, not float.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(horizontal = 16.dp),
                    )
                    listOf(0L to "Any", 1L to "GPS", 2L to "DGPS", 4L to "RTK fixed").forEach { (q, label) ->
                        ChoiceRow(
                            label = label,
                            sub = if (q == 0L) "Today's behaviour: dose on whatever arrives" else null,
                            selected = minQuality == q,
                            enabled = sprayer.connected && minQuality != null,
                        ) { SprayerController.setConfig(SprayerProtocol.KEY_GPS_MIN_QUALITY, q) }
                    }
                }
            }

            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(vertical = 8.dp)) {
                    Text(
                        "Receiver baudrate",
                        style = MaterialTheme.typography.titleMedium,
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    )
                    Text(
                        "Applied at once; the port reopens at the new rate.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(horizontal = 16.dp),
                    )
                    SprayerProtocol.BAUD_RATES.forEachIndexed { i, rate ->
                        ChoiceRow(
                            label = rate.toString(),
                            sub = null,
                            selected = baudIndex == i.toLong(),
                            enabled = sprayer.connected && baudIndex != null,
                        ) { SprayerController.setConfig(SprayerProtocol.KEY_GPS_BAUD, i.toLong()) }
                    }
                }
            }
            if (sprayer.lastMessage.startsWith("Board refused")) {
                Text(sprayer.lastMessage, color = MaterialTheme.colorScheme.error)
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
private fun ChoiceRow(label: String, sub: String?, selected: Boolean, enabled: Boolean, onSelect: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = enabled, onClick = onSelect)
            .padding(start = 8.dp, end = 16.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        RadioButton(selected = selected, onClick = onSelect, enabled = enabled)
        Column {
            Text(label, style = MaterialTheme.typography.bodyLarge)
            if (sub != null) Text(sub, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Suppress("unused")
private fun qualityName(q: Long) = GpsSample.qualityLabel(q.toInt())
