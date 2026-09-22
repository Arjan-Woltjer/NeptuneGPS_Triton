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

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.protocol.SprayerProtocol
import nl.meijworks.spraycomputerld.service.SprayerController

/** The Calibrate menu (NeptuneGPS_Triton#46): the five agreed entries plus the console (#69). */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CalibrateMenuScreen(
    sprayer: SprayerState,
    developerMode: Boolean,
    onBack: () -> Unit,
    onWizard: () -> Unit,
    onPotmeter: () -> Unit,
    onSprayer: () -> Unit,
    onGps: () -> Unit,
    onAdvanced: () -> Unit,
    onConsole: () -> Unit,
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.calibrate_title)) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.action_back))
                    }
                },
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier.padding(padding).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Card(Modifier.fillMaxWidth()) {
                Column {
                    MenuEntry(
                        stringResource(R.string.menu_wizard),
                        stringResource(R.string.menu_wizard_sub),
                        enabled = sprayer.connected,
                        onClick = onWizard,
                    )
                    HorizontalDivider()
                    MenuEntry(
                        stringResource(R.string.menu_potmeter),
                        stringResource(R.string.menu_potmeter_sub),
                        enabled = sprayer.connected,
                        onClick = onPotmeter,
                    )
                    HorizontalDivider()
                    MenuEntry(
                        stringResource(R.string.menu_sprayer),
                        stringResource(R.string.menu_sprayer_sub),
                        enabled = sprayer.connected,
                        onClick = onSprayer,
                    )
                    HorizontalDivider()
                    MenuEntry(
                        stringResource(R.string.menu_gps),
                        stringResource(R.string.menu_gps_sub),
                        enabled = sprayer.connected,
                        onClick = onGps,
                    )
                    HorizontalDivider()
                    MenuEntry(
                        stringResource(R.string.menu_advanced),
                        stringResource(R.string.menu_advanced_sub),
                        enabled = sprayer.connected,
                        onClick = onAdvanced,
                    )
                    HorizontalDivider()
                    HorizontalDivider()
                    BuzzerRow(sprayer)
                    // Console writes arbitrary protocol lines to the board;
                    // an operator who types PWM RUN by accident is driving the
                    // pump from a text field (NeptuneGPS_Triton#124).
                    if (developerMode) {
                        HorizontalDivider()
                        MenuEntry(
                            stringResource(R.string.menu_console),
                            stringResource(R.string.menu_console_sub),
                            enabled = sprayer.connected,
                            onClick = onConsole,
                        )
                    }
                }
            }
            Text(
                if (sprayer.connected) stringResource(R.string.calibrate_hint_connected)
                else stringResource(R.string.calibrate_hint_disconnected),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

/**
 * The board's deviation buzzer on OUT4 (NeptuneGPS_Triton#72). A board-side
 * setting, so it holds without the app; the phone alarm is separate.
 */
@Composable
private fun BuzzerRow(sprayer: SprayerState) {
    val value = sprayer.config[SprayerProtocol.KEY_BUZZER]
    val enabled = sprayer.connected && value != null
    val alpha = if (enabled) 1f else 0.45f
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = enabled) { SprayerController.setConfig(SprayerProtocol.KEY_BUZZER, if (value == 1L) 0L else 1L) }
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(
                stringResource(R.string.buzzer_title),
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = alpha),
            )
            Text(
                stringResource(R.string.buzzer_sub),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = alpha),
            )
        }
        Switch(
            checked = value == 1L,
            onCheckedChange = { SprayerController.setConfig(SprayerProtocol.KEY_BUZZER, if (it) 1L else 0L) },
            enabled = enabled,
        )
    }
}

@Composable
private fun MenuEntry(title: String, subtitle: String, enabled: Boolean, onClick: () -> Unit = {}) {
    val alpha = if (enabled) 1f else 0.45f
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurface.copy(alpha = alpha))
            Text(
                subtitle,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = alpha),
            )
        }
        Icon(
            Icons.AutoMirrored.Filled.KeyboardArrowRight,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = alpha),
        )
    }
}
