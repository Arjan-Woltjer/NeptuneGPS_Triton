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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.isRes
import nl.meijworks.spraycomputerld.textOrEmpty
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
                title = { Text(stringResource(R.string.gps_title)) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.action_back))
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
                    val context = LocalContext.current
                    val none = stringResource(R.string.value_none)
                    Text(stringResource(R.string.gps_live), style = MaterialTheme.typography.titleMedium)
                    DetailRow(stringResource(R.string.gps_fix), g?.qualityText?.resolve(context) ?: none)
                    DetailRow(
                        stringResource(R.string.detail_fix_age),
                        when {
                            g == null -> none
                            g.fixAgeMs == null -> stringResource(R.string.detail_no_fix_yet)
                            else -> stringResource(R.string.value_seconds, g.fixAgeMs / 1000.0)
                        },
                    )
                    DetailRow(
                        stringResource(R.string.gps_speed),
                        sprayer.status?.let { stringResource(R.string.value_speed_kmh, it.speedKmh) } ?: none,
                    )
                    DetailRow(
                        stringResource(R.string.detail_position),
                        g?.takeIf { it.hasPosition }?.let { stringResource(R.string.value_position, it.latitude, it.longitude) } ?: none,
                    )
                }
            }

            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(vertical = 8.dp)) {
                    Text(
                        stringResource(R.string.gps_min_quality),
                        style = MaterialTheme.typography.titleMedium,
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    )
                    Text(
                        stringResource(R.string.gps_min_quality_hint),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(horizontal = 16.dp),
                    )
                    val anySub = stringResource(R.string.gps_any_sub)
                    val qualities = listOf(
                        0L to stringResource(R.string.gps_quality_any),
                        1L to stringResource(R.string.gps_quality_gps),
                        2L to stringResource(R.string.gps_quality_dgps),
                        4L to stringResource(R.string.gps_quality_rtk_fixed),
                    )
                    qualities.forEach { (q, label) ->
                        ChoiceRow(
                            label = label,
                            sub = if (q == 0L) anySub else null,
                            selected = minQuality == q,
                            enabled = sprayer.connected && minQuality != null,
                        ) { SprayerController.setConfig(SprayerProtocol.KEY_GPS_MIN_QUALITY, q) }
                    }
                }
            }

            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(vertical = 8.dp)) {
                    Text(
                        stringResource(R.string.gps_baudrate),
                        style = MaterialTheme.typography.titleMedium,
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    )
                    Text(
                        stringResource(R.string.gps_baudrate_hint),
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
            if (sprayer.lastMessage.isRes(R.string.msg_board_refused)) {
                Text(sprayer.lastMessage.textOrEmpty(), color = MaterialTheme.colorScheme.error)
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

