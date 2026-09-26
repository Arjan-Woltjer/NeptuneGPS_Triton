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

import android.content.Intent
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
import androidx.compose.material.icons.automirrored.filled.VolumeUp
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import nl.meijworks.spraycomputerld.BuildConfig
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.Settings
import nl.meijworks.spraycomputerld.SettingsState
import nl.meijworks.spraycomputerld.audio.AlarmSound
import nl.meijworks.spraycomputerld.service.LogExport
import nl.meijworks.spraycomputerld.service.SprayerController

/** Taps on the build row that turn developer mode on; the Android convention. */
private const val DEVELOPER_TAPS = 7

/** Phone-side settings: how the deviation alarm sounds, screen and battery. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    settings: SettingsState,
    serviceRunning: Boolean,
    onBack: () -> Unit,
    onBatteryOptimizations: () -> Unit,
    isIgnoringBatteryOptimizations: () -> Boolean,
    onShowIntroduction: () -> Unit,
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.settings_title)) },
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
            AlarmCard(settings, serviceRunning)
            OptionsCard(settings, onBatteryOptimizations, isIgnoringBatteryOptimizations, onShowIntroduction)
            if (settings.developerMode) DeveloperCard()
            DiagnosticsCard()
            AboutCard()
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
private fun AlarmCard(settings: SettingsState, serviceRunning: Boolean) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(vertical = 8.dp)) {
            Text(
                stringResource(R.string.settings_alarm_card),
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
            )
            SwitchRow(
                stringResource(R.string.settings_alarm_switch),
                stringResource(R.string.settings_alarm_switch_sub),
                settings.alarmEnabled,
                Modifier.padding(horizontal = 16.dp),
            ) { Settings.setAlarmEnabled(it) }
            HorizontalDivider(Modifier.padding(vertical = 4.dp))
            AlarmSound.entries.forEach { sound ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { Settings.setSound(sound) }
                        .padding(start = 8.dp, end = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    RadioButton(selected = settings.sound == sound, onClick = { Settings.setSound(sound) })
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(sound.labelRes), style = MaterialTheme.typography.bodyLarge)
                        Text(
                            stringResource(sound.descriptionRes),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    IconButton(onClick = { SprayerController.previewSound(sound) }, enabled = serviceRunning) {
                        Icon(
                            Icons.AutoMirrored.Filled.VolumeUp,
                            contentDescription = stringResource(R.string.settings_preview, stringResource(sound.labelRes)),
                        )
                    }
                }
            }
            if (!serviceRunning) {
                Text(
                    stringResource(R.string.settings_connect_to_preview),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp),
                )
            }
            Column(Modifier.padding(horizontal = 16.dp)) {
                Text(stringResource(R.string.settings_volume), style = MaterialTheme.typography.labelLarge)
                Slider(
                    value = settings.volume,
                    onValueChange = { Settings.setVolume(it) },
                    valueRange = 0.05f..1f,
                )
            }
        }
    }
}

@Composable
private fun OptionsCard(
    settings: SettingsState,
    onBatteryOptimizations: () -> Unit,
    isIgnoringBatteryOptimizations: () -> Boolean,
    onShowIntroduction: () -> Unit,
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(stringResource(R.string.settings_phone_card), style = MaterialTheme.typography.titleMedium)
            SwitchRow(stringResource(R.string.settings_vibrate), null, settings.vibrate) { Settings.setVibrate(it) }
            SwitchRow(
                stringResource(R.string.settings_keep_screen_on),
                stringResource(R.string.settings_keep_screen_on_sub),
                settings.keepScreenOn,
            ) { Settings.setKeepScreenOn(it) }
            HorizontalDivider()
            Spacer(Modifier.height(4.dp))
            val ignoring = isIgnoringBatteryOptimizations()
            Text(
                if (ignoring) stringResource(R.string.settings_battery_ok)
                else stringResource(R.string.settings_battery_hint),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            if (!ignoring) {
                OutlinedButton(onClick = onBatteryOptimizations) { Text(stringResource(R.string.settings_battery_button)) }
            }
            HorizontalDivider()
            Spacer(Modifier.height(4.dp))
            OutlinedButton(onClick = onShowIntroduction) { Text(stringResource(R.string.settings_show_intro)) }
        }
    }
}

@Composable
private fun SwitchRow(
    title: String,
    subtitle: String?,
    checked: Boolean,
    modifier: Modifier = Modifier,
    onChange: (Boolean) -> Unit,
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .clickable { onChange(!checked) }
            .padding(vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            if (subtitle != null) {
                Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

/**
 * Export the log (NeptuneGPS_Triton#128). When something goes wrong in a field
 * there is otherwise no evidence afterwards, and this app has no crash
 * reporter by deliberate choice: the users are few and reachable, so a file
 * they send on purpose beats aggregate statistics and a data-collection
 * disclosure.
 *
 * The file is written to the app's cache and handed to a share target. Nothing
 * leaves the device unless the operator picks somewhere to send it.
 */
@Composable
private fun DiagnosticsCard() {
    val context = LocalContext.current
    val sprayer by SprayerController.state.collectAsStateWithLifecycle()
    val subject = stringResource(R.string.diagnostics_subject)
    val chooser = stringResource(R.string.diagnostics_chooser)
    var failure by remember { mutableStateOf<String?>(null) }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(stringResource(R.string.diagnostics_card), style = MaterialTheme.typography.titleMedium)
            Text(
                stringResource(R.string.diagnostics_hint),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            OutlinedButton(onClick = {
                failure = try {
                    val file = LogExport.write(context, state = sprayer)
                    context.startActivity(
                        Intent.createChooser(LogExport.shareIntent(context, file, subject), chooser)
                    )
                    null
                } catch (e: Exception) {
                    // A device with nothing that accepts text/plain, or a
                    // cache that cannot be written. Either way, say so rather
                    // than looking like the button did nothing.
                    e.message ?: e.javaClass.simpleName
                }
            }) {
                Text(stringResource(R.string.diagnostics_export))
            }
            failure?.let {
                Text(
                    stringResource(R.string.diagnostics_failed, it),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error,
                )
            }
        }
    }
}

/** Visible only once developer mode is on, so it can be switched off again. */
@Composable
private fun DeveloperCard() {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            SwitchRow(
                stringResource(R.string.settings_developer),
                stringResource(R.string.settings_developer_sub),
                true,
            ) { Settings.setDeveloperMode(it) }
        }
    }
}

/**
 * Which build this is, for a bug report: version name, code and commit.
 *
 * Also the way in to developer mode, by the usual Android convention of
 * repeated taps on the build row: an operator does not find it by accident,
 * and nobody has to be talked through a hidden gesture they could trip over.
 */
@Composable
private fun AboutCard() {
    var taps by remember { mutableIntStateOf(0) }
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(stringResource(R.string.settings_about), style = MaterialTheme.typography.titleMedium)
            Text(
                stringResource(
                    R.string.settings_about_build,
                    BuildConfig.VERSION_NAME,
                    BuildConfig.VERSION_CODE,
                    BuildConfig.GIT_SHA,
                ),
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.clickable {
                    taps++
                    if (taps >= DEVELOPER_TAPS) {
                        taps = 0
                        Settings.setDeveloperMode(true)
                    }
                },
            )
            Text(
                stringResource(R.string.settings_about_blurb),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(4.dp))
            Text(stringResource(R.string.settings_licence), style = MaterialTheme.typography.bodySmall)
            // Compose, AndroidX and the coroutines library are all Apache-2.0.
            // Listed rather than reproduced: the app ships no third-party
            // source, and the full texts live with the projects themselves.
            Text(
                stringResource(R.string.settings_notices),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
