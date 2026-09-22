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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import nl.meijworks.spraycomputerld.BuildConfig
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.Settings
import nl.meijworks.spraycomputerld.SettingsState
import nl.meijworks.spraycomputerld.audio.AlarmSound
import nl.meijworks.spraycomputerld.service.SprayerController

/** Phone-side settings: how the deviation alarm sounds, screen and battery. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    settings: SettingsState,
    serviceRunning: Boolean,
    onBack: () -> Unit,
    onBatteryOptimizations: () -> Unit,
    isIgnoringBatteryOptimizations: () -> Boolean,
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
            OptionsCard(settings, onBatteryOptimizations, isIgnoringBatteryOptimizations)
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

/** Which build this is, for a bug report: version name, code and commit. */
@Composable
private fun AboutCard() {
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
            )
            Text(
                stringResource(R.string.settings_about_blurb),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
