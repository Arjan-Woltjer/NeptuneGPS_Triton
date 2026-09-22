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

package nl.meijworks.spraycomputerld.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.protocol.GpsSample
import nl.meijworks.spraycomputerld.protocol.SprayerProtocol
import nl.meijworks.spraycomputerld.service.SprayerController

/**
 * What the board holds: the two calibration tables, the settings, and a
 * pump point's flow correction (serial menu option 4). The console has its
 * own screen (ConsoleScreen).
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AdvancedScreen(sprayer: SprayerState, developerMode: Boolean, onBack: () -> Unit) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.advanced_title)) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.action_back))
                    }
                },
                actions = {
                    IconButton(onClick = { SprayerController.refresh() }, enabled = sprayer.connected) {
                        Icon(Icons.Filled.Refresh, contentDescription = stringResource(R.string.advanced_reread))
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
            TablesCard(sprayer, developerMode)
            SettingsValuesCard(sprayer)
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
private fun TablesCard(sprayer: SprayerState, developerMode: Boolean) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(stringResource(R.string.advanced_knob_calibration), style = MaterialTheme.typography.titleMedium)
            if (sprayer.dosePoints.isEmpty()) {
                Text(stringResource(R.string.value_not_read_yet), color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                TableHeader(
                    stringResource(R.string.advanced_column_index),
                    stringResource(R.string.advanced_column_analog),
                    stringResource(R.string.advanced_column_dose),
                )
                sprayer.dosePoints.forEach { p ->
                    TableRow("${p.index + 1}", "${p.analog}", "${p.doseLha}")
                }
            }
            Spacer(Modifier.height(8.dp))
            Text(stringResource(R.string.advanced_pump_curve), style = MaterialTheme.typography.titleMedium)
            if (sprayer.pwmPoints.isEmpty()) {
                Text(stringResource(R.string.value_not_read_yet), color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                TableHeader(
                    stringResource(R.string.advanced_column_index),
                    stringResource(R.string.advanced_column_pwm),
                    stringResource(R.string.advanced_column_flow),
                )
                sprayer.pwmPoints.forEach { p ->
                    TableRow("${p.index + 1}", "${p.pwm}", "${p.flowMlMin}")
                }
                // Reading the tables is fair game for an operator; writing a
                // point's flow by hand, outside the wizard, is not.
                if (developerMode) PwmPointEditor(sprayer)
            }
        }
    }
}

@Composable
private fun SettingsValuesCard(sprayer: SprayerState) {
    val c = sprayer.config
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            val context = LocalContext.current
            val none = stringResource(R.string.value_none)
            Text(stringResource(R.string.advanced_board_settings), style = MaterialTheme.typography.titleMedium)
            if (c.isEmpty()) {
                Text(stringResource(R.string.value_not_read_yet), color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                DetailRow(
                    stringResource(R.string.advanced_width),
                    c[SprayerProtocol.KEY_WIDTH_CM]?.let { stringResource(R.string.advanced_value_cm, it) } ?: none,
                )
                DetailRow(
                    stringResource(R.string.advanced_guidance_timeout),
                    c[SprayerProtocol.KEY_GUIDANCE_MS]?.let { stringResource(R.string.advanced_value_ms, it) } ?: none,
                )
                DetailRow(
                    stringResource(R.string.advanced_gps_baudrate),
                    c[SprayerProtocol.KEY_GPS_BAUD]?.let { idx ->
                        SprayerProtocol.BAUD_RATES.getOrNull(idx.toInt())?.toString()
                            ?: stringResource(R.string.advanced_baud_index, idx)
                    } ?: none,
                )
                DetailRow(
                    stringResource(R.string.advanced_min_fix),
                    c[SprayerProtocol.KEY_GPS_MIN_QUALITY]?.let { q ->
                        if (q == 0L) stringResource(R.string.gps_quality_any_lowercase)
                        else GpsSample.qualityText(q.toInt()).resolve(context)
                    } ?: none,
                )
            }
            sprayer.protocolVersion?.let {
                DetailRow(stringResource(R.string.advanced_protocol), stringResource(R.string.advanced_protocol_value, it))
            }
            sprayer.firmwareVersion?.let { DetailRow(stringResource(R.string.advanced_firmware), it) }
        }
    }
}

@Composable
private fun PwmPointEditor(sprayer: SprayerState) {
    var pointText by rememberSaveable { mutableStateOf("") }
    var flowText by rememberSaveable { mutableStateOf("") }
    val point = pointText.toIntOrNull()?.let { n -> sprayer.pwmPoints.firstOrNull { it.index == n - 1 } }
    val flow = flowText.toIntOrNull()
    Spacer(Modifier.height(4.dp))
    Text(stringResource(R.string.advanced_correct_flow), style = MaterialTheme.typography.labelLarge)
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedTextField(
            value = pointText,
            onValueChange = { pointText = it.filter { c -> c.isDigit() } },
            label = { Text(stringResource(R.string.advanced_column_index)) },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.weight(0.6f),
        )
        OutlinedTextField(
            value = flowText,
            onValueChange = { flowText = it.filter { c -> c.isDigit() } },
            label = { Text(stringResource(R.string.advanced_field_ml_min)) },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.weight(1f),
        )
        OutlinedButton(
            onClick = { if (point != null && flow != null) SprayerController.editPwmPointFlow(point.index, flow) },
            enabled = sprayer.connected && point != null && flow != null && flow in 1..4000,
        ) { Text(stringResource(R.string.action_save)) }
    }
}

@Composable
private fun TableHeader(a: String, b: String, c: String) {
    Row(Modifier.fillMaxWidth()) {
        Text(a, Modifier.weight(0.5f), style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(b, Modifier.weight(1f), style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(c, Modifier.weight(1.5f), style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun TableRow(a: String, b: String, c: String) {
    Row(Modifier.fillMaxWidth()) {
        Text(a, Modifier.weight(0.5f), fontWeight = FontWeight.Medium)
        Text(b, Modifier.weight(1f))
        Text(c, Modifier.weight(1.5f))
    }
}
