package com.meijworks.loofdoes.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.Send
import androidx.compose.material.icons.filled.Refresh
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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import com.meijworks.loofdoes.SprayerState
import com.meijworks.loofdoes.protocol.GpsSample
import com.meijworks.loofdoes.protocol.SprayerProtocol
import com.meijworks.loofdoes.service.SprayerController

/**
 * Read-only view of what the board holds, plus a console that sends raw
 * protocol lines and shows the last lines in both directions. Editing comes
 * with #52.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AdvancedScreen(sprayer: SprayerState, onBack: () -> Unit) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Advanced") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = { SprayerController.refresh() }, enabled = sprayer.connected) {
                        Icon(Icons.Filled.Refresh, contentDescription = "Re-read from the board")
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
            TablesCard(sprayer)
            SettingsValuesCard(sprayer)
            ConsoleCard(sprayer)
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
private fun TablesCard(sprayer: SprayerState) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Knob calibration", style = MaterialTheme.typography.titleMedium)
            if (sprayer.dosePoints.isEmpty()) {
                Text("Not read yet", color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                TableHeader("#", "Analog", "Dose (l/ha)")
                sprayer.dosePoints.forEach { p ->
                    TableRow("${p.index + 1}", "${p.analog}", "${p.doseLha}")
                }
            }
            Spacer(Modifier.height(8.dp))
            Text("Pump curve", style = MaterialTheme.typography.titleMedium)
            if (sprayer.pwmPoints.isEmpty()) {
                Text("Not read yet", color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                TableHeader("#", "PWM", "Flow (ml/min)")
                sprayer.pwmPoints.forEach { p ->
                    TableRow("${p.index + 1}", "${p.pwm}", "${p.flowMlMin}")
                }
            }
        }
    }
}

@Composable
private fun SettingsValuesCard(sprayer: SprayerState) {
    val c = sprayer.config
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text("Board settings", style = MaterialTheme.typography.titleMedium)
            if (c.isEmpty()) {
                Text("Not read yet", color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                DetailRow("Width", c[SprayerProtocol.KEY_WIDTH_CM]?.let { "$it cm" } ?: "–")
                DetailRow("Guidance timeout", c[SprayerProtocol.KEY_GUIDANCE_MS]?.let { "$it ms" } ?: "–")
                DetailRow(
                    "GPS baudrate",
                    c[SprayerProtocol.KEY_GPS_BAUD]?.let { idx ->
                        SprayerProtocol.BAUD_RATES.getOrNull(idx.toInt())?.toString() ?: "index $idx"
                    } ?: "–",
                )
                DetailRow(
                    "Minimum fix to dose",
                    c[SprayerProtocol.KEY_GPS_MIN_QUALITY]?.let { q ->
                        if (q == 0L) "any" else GpsSample.qualityLabel(q.toInt())
                    } ?: "–",
                )
            }
            sprayer.protocolVersion?.let { DetailRow("Protocol", "v$it") }
            sprayer.firmwareVersion?.let { DetailRow("Firmware", it) }
        }
    }
}

@Composable
private fun ConsoleCard(sprayer: SprayerState) {
    var command by rememberSaveable { mutableStateOf("") }
    fun send() {
        if (command.isNotBlank()) {
            SprayerController.sendRaw(command)
            command = ""
        }
    }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Console", style = MaterialTheme.typography.titleMedium)
            Text(
                "Raw protocol lines, for the bench. Try PING, CAL GET, CFG GET.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Row(verticalAlignment = Alignment.CenterVertically) {
                OutlinedTextField(
                    value = command,
                    onValueChange = { command = it },
                    modifier = Modifier.weight(1f),
                    singleLine = true,
                    placeholder = { Text("PING") },
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send),
                    keyboardActions = KeyboardActions(onSend = { send() }),
                    enabled = sprayer.connected,
                )
                Spacer(Modifier.width(8.dp))
                Button(onClick = { send() }, enabled = sprayer.connected && command.isNotBlank()) {
                    Icon(Icons.AutoMirrored.Filled.Send, contentDescription = "Send")
                }
            }
            Column {
                sprayer.log.takeLast(15).forEach { line ->
                    Text(
                        line,
                        fontFamily = FontFamily.Monospace,
                        style = MaterialTheme.typography.bodySmall,
                        color = if (line.startsWith(">")) MaterialTheme.colorScheme.primary
                        else MaterialTheme.colorScheme.onSurface,
                    )
                }
            }
        }
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
