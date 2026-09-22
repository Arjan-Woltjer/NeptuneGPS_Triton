package nl.meijworks.spraycomputerld.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.isRes
import nl.meijworks.spraycomputerld.textOrEmpty
import nl.meijworks.spraycomputerld.protocol.SprayerProtocol
import nl.meijworks.spraycomputerld.service.SprayerController

/** Width and guidance timeout, stored on the board (ConfigSprayer). */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SprayerConfigScreen(sprayer: SprayerState, onBack: () -> Unit) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.sprayer_title)) },
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
            NumberSetting(
                title = stringResource(R.string.sprayer_width),
                unit = stringResource(R.string.unit_cm),
                hint = stringResource(R.string.sprayer_width_hint),
                key = SprayerProtocol.KEY_WIDTH_CM,
                current = sprayer.config[SprayerProtocol.KEY_WIDTH_CM],
                range = 50L..5000L,
                enabled = sprayer.connected,
            )
            NumberSetting(
                title = stringResource(R.string.sprayer_guidance),
                unit = stringResource(R.string.unit_ms),
                hint = stringResource(R.string.sprayer_guidance_hint),
                key = SprayerProtocol.KEY_GUIDANCE_MS,
                current = sprayer.config[SprayerProtocol.KEY_GUIDANCE_MS],
                range = 500L..10000L,
                enabled = sprayer.connected,
            )
            if (sprayer.lastMessage.isRes(R.string.msg_board_refused)) {
                Text(sprayer.lastMessage.textOrEmpty(), color = MaterialTheme.colorScheme.error)
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
fun NumberSetting(
    title: String,
    unit: String,
    hint: String,
    key: String,
    current: Long?,
    range: LongRange,
    enabled: Boolean,
) {
    var text by rememberSaveable(current) { mutableStateOf(current?.toString() ?: "") }
    val value = text.toLongOrNull()
    val changed = value != null && value != current
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            Text(
                stringResource(
                    R.string.setting_on_board_now,
                    current?.let { stringResource(R.string.setting_value_with_unit, it, unit) }
                        ?: stringResource(R.string.setting_not_read_yet),
                ),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            OutlinedTextField(
                value = text,
                onValueChange = { text = it.filter { c -> c.isDigit() } },
                label = { Text(stringResource(R.string.setting_field_label, title, unit)) },
                singleLine = true,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
                isError = value != null && value !in range,
            )
            Text(hint, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Button(
                onClick = { value?.let { SprayerController.setConfig(key, it) } },
                enabled = enabled && changed && value in range,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(stringResource(R.string.action_save_to_board)) }
        }
    }
}
