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
import nl.meijworks.spraycomputerld.protocol.WizardMath
import nl.meijworks.spraycomputerld.service.SprayerController

/**
 * What the board holds: the two calibration tables, both editable in place
 * (NeptuneGPS_Triton#138), and the settings, read-only. The console has its
 * own screen (ConsoleScreen).
 *
 * The board's own limits, so a value it would refuse cannot be sent:
 * RemoteSprayer::handleCal takes an analog value and a duty in 0..4095, a
 * dose in 1..10000 l/ha and a flow in 0..4000 ml/min.
 */
private const val PWM_MAX_DUTY = 4095
private const val MAX_DOSE_LHA = 10000
private const val MAX_FLOW_ML_MIN = 4000

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AdvancedScreen(sprayer: SprayerState, onBack: () -> Unit) {
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
            DosePointsCard(sprayer)
            PwmPointsCard(sprayer)
            SettingsValuesCard(sprayer)
            Spacer(Modifier.height(24.dp))
        }
    }
}

/**
 * The knob table, editable in place (NeptuneGPS_Triton#138). The wizard is
 * the way these are normally produced; this is for correcting one afterwards
 * without walking the whole procedure again.
 */
@Composable
private fun DosePointsCard(sprayer: SprayerState) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(stringResource(R.string.advanced_edit_dose), style = MaterialTheme.typography.titleMedium)
            if (sprayer.dosePoints.isEmpty()) {
                Text(stringResource(R.string.value_not_read_yet), color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                sprayer.dosePoints.forEach { p ->
                    PointEditor(
                        title = stringResource(
                            R.string.potmeter_point,
                            p.index + 1,
                            WizardMath.DOSE_LABELS.getOrNull(p.index)?.let { stringResource(it) }.orEmpty(),
                        ),
                        firstLabel = stringResource(R.string.advanced_column_analog),
                        firstValue = p.analog,
                        firstRange = 0..PWM_MAX_DUTY,
                        secondLabel = stringResource(R.string.unit_lha),
                        secondValue = p.doseLha,
                        secondRange = 1..MAX_DOSE_LHA,
                        enabled = sprayer.connected,
                    ) { analog, dose -> SprayerController.editDosePoint(p.index, analog, dose) }
                }
            }
        }
    }
}

/** The pump curve, same shape. Replaces the old flow-only correction field. */
@Composable
private fun PwmPointsCard(sprayer: SprayerState) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(stringResource(R.string.advanced_edit_pump), style = MaterialTheme.typography.titleMedium)
            if (sprayer.pwmPoints.isEmpty()) {
                Text(stringResource(R.string.value_not_read_yet), color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                sprayer.pwmPoints.forEach { p ->
                    PointEditor(
                        title = stringResource(R.string.advanced_point_number, p.index + 1),
                        firstLabel = stringResource(R.string.advanced_column_pwm),
                        firstValue = p.pwm,
                        firstRange = 0..PWM_MAX_DUTY,
                        secondLabel = stringResource(R.string.advanced_field_ml_min),
                        secondValue = p.flowMlMin,
                        secondRange = 0..MAX_FLOW_ML_MIN,
                        enabled = sprayer.connected,
                    ) { pwm, flow -> SprayerController.editPwmPoint(p.index, pwm, flow) }
                }
            }
        }
    }
}

/**
 * One calibration point: two numbers and a Save that commits just that point.
 *
 * Save is offered only for a value the board will accept, so a rejected edit
 * is the exception rather than the way you find out the range. The fields
 * re-seed from the board's own values, so a Save followed by the board's
 * CAL GET leaves the row showing what was actually stored, not what was typed.
 */
@Composable
private fun PointEditor(
    title: String,
    firstLabel: String,
    firstValue: Int,
    firstRange: IntRange,
    secondLabel: String,
    secondValue: Int,
    secondRange: IntRange,
    enabled: Boolean,
    onSave: (Int, Int) -> Unit,
) {
    var firstText by rememberSaveable(firstValue) { mutableStateOf(firstValue.toString()) }
    var secondText by rememberSaveable(secondValue) { mutableStateOf(secondValue.toString()) }
    val first = firstText.toIntOrNull()
    val second = secondText.toIntOrNull()
    val valid = first in firstRange && second in secondRange
    val changed = first != firstValue || second != secondValue

    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(title, style = MaterialTheme.typography.labelLarge)
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedTextField(
                value = firstText,
                onValueChange = { firstText = it.filter { c -> c.isDigit() } },
                label = { Text(firstLabel) },
                singleLine = true,
                isError = first !in firstRange,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.weight(1f),
            )
            OutlinedTextField(
                value = secondText,
                onValueChange = { secondText = it.filter { c -> c.isDigit() } },
                label = { Text(secondLabel) },
                singleLine = true,
                isError = second !in secondRange,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.weight(1f),
            )
            OutlinedButton(
                onClick = { if (first != null && second != null) onSave(first, second) },
                enabled = enabled && valid && changed,
            ) { Text(stringResource(R.string.action_save)) }
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
