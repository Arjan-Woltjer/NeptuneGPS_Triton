package nl.meijworks.spraycomputerld.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
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
import nl.meijworks.spraycomputerld.protocol.WizardMath
import nl.meijworks.spraycomputerld.service.SprayerController
import nl.meijworks.spraycomputerld.service.WizardMode

/**
 * The knob half of the calibration on its own: the current three points,
 * redo them all, or redo one.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PotmeterScreen(sprayer: SprayerState, onBack: () -> Unit, onStartWizard: () -> Unit) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.potmeter_title)) },
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
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(stringResource(R.string.potmeter_current), style = MaterialTheme.typography.titleMedium)
                    Text(
                        stringResource(
                            R.string.potmeter_live_reading,
                            sprayer.status?.raw?.toString() ?: stringResource(R.string.value_none),
                        ),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    if (sprayer.dosePoints.isEmpty()) {
                        Text(stringResource(R.string.value_not_read_yet), color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    sprayer.dosePoints.forEach { p ->
                        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) {
                                Text(
                                    stringResource(
                                        R.string.potmeter_point,
                                        p.index + 1,
                                        WizardMath.DOSE_LABELS.getOrNull(p.index)?.let { stringResource(it) }.orEmpty(),
                                    ),
                                    fontWeight = FontWeight.Medium,
                                )
                                Text(
                                    stringResource(R.string.potmeter_point_values, p.analog, p.doseLha),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            OutlinedButton(
                                onClick = {
                                    SprayerController.startWizard(WizardMode.DOSE_SINGLE, p.index)
                                    onStartWizard()
                                },
                                enabled = sprayer.connected,
                            ) { Text(stringResource(R.string.action_redo)) }
                        }
                    }
                }
            }
            Button(
                onClick = {
                    SprayerController.startWizard(WizardMode.DOSE_ONLY)
                    onStartWizard()
                },
                enabled = sprayer.connected,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(stringResource(R.string.potmeter_redo_all)) }
            Text(
                stringResource(R.string.potmeter_hint),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(24.dp))
        }
    }
}
