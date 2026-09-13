package com.meijworks.loofdoes.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.Send
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.meijworks.loofdoes.SprayerState
import com.meijworks.loofdoes.service.SprayerController

/**
 * The console on a screen of its own (NeptuneGPS_Triton#69): the log fills
 * the screen with the newest line at the bottom, the input stays fixed
 * below it, the two telemetry switches sit above the log, and the whole log
 * can be copied for an issue.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConsoleScreen(sprayer: SprayerState, onBack: () -> Unit) {
    val showTelemetry by SprayerController.showTelemetry.collectAsStateWithLifecycle()
    val nmea by SprayerController.nmeaEnabled.collectAsStateWithLifecycle()
    val clipboard = LocalClipboardManager.current
    val listState = rememberLazyListState()
    var command by rememberSaveable { mutableStateOf("") }

    // Follow the newest line, as a terminal does.
    LaunchedEffect(sprayer.log.size) {
        if (sprayer.log.isNotEmpty()) listState.animateScrollToItem(sprayer.log.size - 1)
    }

    fun send() {
        if (command.isNotBlank()) {
            SprayerController.sendRaw(command)
            command = ""
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Console") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(
                        onClick = { clipboard.setText(AnnotatedString(sprayer.log.joinToString("\n"))) },
                        enabled = sprayer.log.isNotEmpty(),
                    ) {
                        Icon(Icons.Filled.ContentCopy, contentDescription = "Copy the whole log")
                    }
                    IconButton(onClick = { SprayerController.clearLog() }, enabled = sprayer.log.isNotEmpty()) {
                        Icon(Icons.Filled.Delete, contentDescription = "Clear the log")
                    }
                },
            )
        }
    ) { padding ->
        Column(Modifier.padding(padding).fillMaxSize()) {
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                SwitchChip("Status and GPS lines", showTelemetry) { SprayerController.setShowTelemetry(it) }
                SwitchChip("GPS raw sentences", nmea, enabled = sprayer.connected) { SprayerController.setNmea(it) }
            }
            HorizontalDivider()

            SelectionContainer(Modifier.weight(1f)) {
                LazyColumn(
                    state = listState,
                    modifier = Modifier.fillMaxSize().padding(horizontal = 16.dp, vertical = 8.dp),
                ) {
                    items(sprayer.log) { line ->
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

            HorizontalDivider()
            Row(
                Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                OutlinedTextField(
                    value = command,
                    onValueChange = { command = it },
                    modifier = Modifier.weight(1f),
                    singleLine = true,
                    placeholder = { Text("PING, CAL GET, CFG GET, TELEM S 0 …") },
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send),
                    keyboardActions = KeyboardActions(onSend = { send() }),
                    enabled = sprayer.connected,
                )
                Spacer(Modifier.width(8.dp))
                Button(onClick = { send() }, enabled = sprayer.connected && command.isNotBlank()) {
                    Icon(Icons.AutoMirrored.Filled.Send, contentDescription = "Send")
                }
            }
        }
    }
}

@Composable
private fun SwitchChip(label: String, checked: Boolean, enabled: Boolean = true, onChange: (Boolean) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Switch(checked = checked, onCheckedChange = onChange, enabled = enabled)
        Spacer(Modifier.width(6.dp))
        Text(label, style = MaterialTheme.typography.bodySmall)
    }
}
