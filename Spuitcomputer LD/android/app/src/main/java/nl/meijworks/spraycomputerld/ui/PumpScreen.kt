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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.service.SprayerController
import nl.meijworks.spraycomputerld.service.WizardMode

/**
 * The pump curve on its own, in the same shape as Potmeterkalibratie
 * (NeptuneGPS_Triton#179): the current points and their graph, then redo
 * one point, redo the start point, or redo the lot.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PumpScreen(sprayer: SprayerState, onBack: () -> Unit, onStartWizard: () -> Unit) {
    fun start(mode: WizardMode, index: Int = 0) {
        SprayerController.startWizard(mode, index)
        onStartWizard()
    }
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.pump_title)) },
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
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(stringResource(R.string.pump_current), style = MaterialTheme.typography.titleMedium)
                    if (sprayer.pwmPoints.isEmpty()) {
                        Text(stringResource(R.string.value_not_read_yet), color = MaterialTheme.colorScheme.onSurfaceVariant)
                    } else {
                        CalibrationGraph(
                            points = sprayer.pwmPoints.map { GraphPoint(it.pwm, it.flowMlMin) },
                            xLabel = stringResource(R.string.graph_axis_pwm),
                            yLabel = stringResource(R.string.graph_axis_ml_min),
                        )
                    }
                    sprayer.pwmPoints.forEach { p ->
                        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(stringResource(R.string.pump_point, p.index + 1), fontWeight = FontWeight.Medium)
                                Text(
                                    stringResource(R.string.pump_point_values, p.pwm, p.flowMlMin),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            OutlinedButton(
                                onClick = { start(WizardMode.PUMP_SINGLE, p.index) },
                                enabled = sprayer.connected,
                            ) { Text(stringResource(R.string.action_redo)) }
                        }
                    }
                }
            }
            OutlinedButton(
                onClick = { start(WizardMode.PUMP_START) },
                enabled = sprayer.connected,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(stringResource(R.string.pump_redo_start)) }
            Button(
                onClick = { start(WizardMode.PUMP_ONLY) },
                enabled = sprayer.connected,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(stringResource(R.string.pump_redo_all)) }
            Text(
                stringResource(R.string.pump_hint),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(24.dp))
        }
    }
}
