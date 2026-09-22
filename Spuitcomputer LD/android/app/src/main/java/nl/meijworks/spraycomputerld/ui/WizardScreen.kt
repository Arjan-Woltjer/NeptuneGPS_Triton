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
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import nl.meijworks.spraycomputerld.R
import nl.meijworks.spraycomputerld.SprayerState
import nl.meijworks.spraycomputerld.text
import nl.meijworks.spraycomputerld.protocol.WizardMath
import nl.meijworks.spraycomputerld.service.SprayerController
import nl.meijworks.spraycomputerld.service.WizardMode
import nl.meijworks.spraycomputerld.service.WizardState
import nl.meijworks.spraycomputerld.service.WizardStep

/**
 * One screen for every wizard mode; it renders whatever step the service's
 * CalibrationWizard is at. The step texts follow the serial wizard so the
 * calibration procedure document still applies.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun WizardScreen(sprayer: SprayerState, title: String, onClose: () -> Unit) {
    val w = sprayer.wizard
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(title) },
                navigationIcon = {
                    IconButton(onClick = { SprayerController.cancelWizard(); onClose() }) {
                        Icon(Icons.Filled.Close, contentDescription = stringResource(R.string.action_cancel))
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
            if (w == null) {
                Text(stringResource(R.string.wizard_starting))
            } else {
                Progress(w)
                when (w.step) {
                    WizardStep.STARTING -> Waiting(stringResource(R.string.wizard_taking_calibration))
                    WizardStep.DOSE_CAPTURE -> DoseCapture(w, sprayer)
                    WizardStep.DOSE_ENTER -> DoseEnter(w)
                    WizardStep.PUMP_FIND -> PumpFind(w, sprayer)
                    WizardStep.PUMP_STEP_READY -> PumpStepReady(w)
                    WizardStep.PUMP_RUNNING -> PumpRunning(w)
                    WizardStep.PUMP_ENTER -> PumpEnter(w)
                    WizardStep.SAVING -> Waiting(stringResource(R.string.wizard_saving))
                    WizardStep.DONE -> Done(w, onClose)
                    WizardStep.FAILED -> Failed(w, onClose)
                }
                if (sprayer.pairing) {
                    Text(
                        stringResource(R.string.wizard_pairing),
                        color = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.fillMaxWidth(),
                        textAlign = TextAlign.Center,
                    )
                }
                w.message?.let {
                    Text(
                        it.text(),
                        color = MaterialTheme.colorScheme.error,
                        modifier = Modifier.fillMaxWidth(),
                        textAlign = TextAlign.Center,
                    )
                }
                if (!sprayer.connected && w.step != WizardStep.FAILED && w.step != WizardStep.DONE) {
                    Text(stringResource(R.string.wizard_link_lost), color = MaterialTheme.colorScheme.error)
                }
            }
            Spacer(Modifier.height(24.dp))
        }
    }
}

@Composable
private fun Progress(w: WizardState) {
    val (done, total) = when (w.mode) {
        WizardMode.FULL -> {
            val dose = if (w.step >= WizardStep.PUMP_FIND) WizardMath.DOSE_POINTS else w.doseIndex
            val pump = if (w.step >= WizardStep.PUMP_STEP_READY) w.pwmIndex else 0
            (dose + pump) to (WizardMath.DOSE_POINTS + WizardMath.PWM_STEPS)
        }
        WizardMode.DOSE_ONLY -> w.doseIndex to WizardMath.DOSE_POINTS
        WizardMode.DOSE_SINGLE -> (if (w.step >= WizardStep.SAVING) 1 else 0) to 1
        WizardMode.PUMP_ONLY -> (if (w.step >= WizardStep.PUMP_STEP_READY) w.pwmIndex else 0) to WizardMath.PWM_STEPS
    }
    val fraction = if (w.step == WizardStep.DONE) 1f else done.toFloat() / total
    LinearProgressIndicator(progress = { fraction }, modifier = Modifier.fillMaxWidth())
}

@Composable
private fun Waiting(text: String) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        CircularProgressIndicator(Modifier.width(24.dp).height(24.dp))
        Spacer(Modifier.width(12.dp))
        Text(text)
    }
}

// ---------------------------------------------------------------- knob

@Composable
private fun DoseCapture(w: WizardState, sprayer: SprayerState) {
    val raw = sprayer.status?.raw
    StepCard(
        title = if (w.mode == WizardMode.DOSE_SINGLE) stringResource(R.string.wizard_knob_position_single, w.doseIndex + 1)
        else stringResource(R.string.wizard_knob_position_of, w.doseIndex + 1, WizardMath.DOSE_POINTS),
    ) {
        Text(stringResource(R.string.wizard_set_knob, stringResource(w.doseLabel)))
        BigValue(
            label = stringResource(R.string.wizard_knob_reading),
            value = raw?.toString() ?: stringResource(R.string.value_none),
            unit = stringResource(R.string.unit_of_4095),
        )
        Button(
            onClick = { SprayerController.wizardCaptureDose(raw) },
            enabled = !w.busy && raw != null,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(stringResource(R.string.wizard_capture)) }
    }
}

@Composable
private fun DoseEnter(w: WizardState) {
    var text by rememberSaveable(w.doseIndex) { mutableStateOf("") }
    val value = text.toIntOrNull()
    StepCard(title = stringResource(R.string.wizard_dose_for_position, stringResource(w.doseLabel))) {
        Text(stringResource(R.string.wizard_knob_captured, w.capturedAnalog ?: 0))
        OutlinedTextField(
            value = text,
            onValueChange = { text = it.filter { c -> c.isDigit() } },
            label = { Text(stringResource(R.string.wizard_dose_field)) },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { value?.let { SprayerController.wizardEnterDose(it) } },
            enabled = !w.busy && value != null && value > 0,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(stringResource(R.string.wizard_save_point, w.doseIndex + 1)) }
    }
}

// ---------------------------------------------------------------- pump

@Composable
private fun PumpFind(w: WizardState, sprayer: SprayerState) {
    StepCard(title = stringResource(R.string.wizard_find_start)) {
        Text(stringResource(R.string.wizard_find_start_hint))
        BigValue(
            label = stringResource(R.string.wizard_pump_duty),
            value = w.findDuty.toString(),
            unit = stringResource(R.string.unit_of_4095),
        )
        Slider(
            value = w.findDuty.toFloat(),
            onValueChange = { SprayerController.wizardSetFindDuty(it.toInt()) },
            valueRange = 0f..WizardMath.MAX_DUTY.toFloat(),
            enabled = !w.busy,
        )
        Text(
            stringResource(R.string.wizard_board_reports_duty, sprayer.status?.pumpPwm?.toString() ?: stringResource(R.string.value_none)),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Button(
            onClick = { SprayerController.wizardCaptureStart() },
            enabled = !w.busy && w.findDuty >= WizardMath.MIN_START_DUTY,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(stringResource(R.string.wizard_capture_flowing)) }
    }
}

@Composable
private fun PumpStepReady(w: WizardState) {
    StepCard(title = stringResource(R.string.wizard_point_of, w.pwmIndex + 1, w.pwmSteps.size)) {
        Text(stringResource(R.string.wizard_jug_hint))
        BigValue(
            label = stringResource(R.string.wizard_pump_duty),
            value = w.currentPwmDuty.toString(),
            unit = stringResource(R.string.unit_of_4095),
        )
        Button(
            onClick = { SprayerController.wizardStartRun() },
            enabled = !w.busy,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(stringResource(R.string.wizard_start_run)) }
    }
}

@Composable
private fun PumpRunning(w: WizardState) {
    StepCard(title = stringResource(R.string.wizard_point_running, w.pwmIndex + 1, w.pwmSteps.size)) {
        BigValue(
            label = stringResource(R.string.wizard_remaining),
            value = (w.secondsRemaining ?: 0).toString(),
            unit = stringResource(R.string.unit_seconds_short),
        )
        LinearProgressIndicator(
            progress = { 1f - (w.secondsRemaining ?: 0).toFloat() / WizardMath.RUN_SECONDS },
            modifier = Modifier.fillMaxWidth(),
        )
        Text(
            stringResource(R.string.wizard_screen_may_sleep),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        OutlinedButton(onClick = { SprayerController.wizardStopRun() }, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.wizard_stop_redo))
        }
    }
}

@Composable
private fun PumpEnter(w: WizardState) {
    var text by rememberSaveable(w.pwmIndex) { mutableStateOf("") }
    val value = text.toIntOrNull()
    StepCard(title = stringResource(R.string.wizard_point_volume, w.pwmIndex + 1, w.pwmSteps.size)) {
        Text(stringResource(R.string.wizard_run_complete, w.currentPwmDuty))
        OutlinedTextField(
            value = text,
            onValueChange = { text = it.filter { c -> c.isDigit() } },
            label = { Text(stringResource(R.string.wizard_volume_field)) },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { value?.let { SprayerController.wizardEnterVolume(it) } },
            enabled = !w.busy && value != null && value in 1..WizardMath.MAX_FLOW_ML_MIN,
            modifier = Modifier.fillMaxWidth(),
        ) {
        Text(
            if (w.pwmIndex + 1 < w.pwmSteps.size) stringResource(R.string.wizard_save_and_next)
            else stringResource(R.string.wizard_save_and_finish)
        )
    }
    }
}

// ---------------------------------------------------------------- end

@Composable
private fun Done(w: WizardState, onClose: () -> Unit) {
    StepCard(title = stringResource(R.string.wizard_saved_on_board)) {
        if (w.doseCaptured.isNotEmpty()) {
            Text(stringResource(R.string.wizard_knob_positions), fontWeight = FontWeight.SemiBold)
            w.doseCaptured.forEach {
                Text(stringResource(R.string.wizard_dose_summary, it.index + 1, it.analog, it.doseLha))
            }
        }
        if (w.pwmCaptured.isNotEmpty()) {
            Text(stringResource(R.string.wizard_pump_curve), fontWeight = FontWeight.SemiBold)
            w.pwmCaptured.forEach {
                Text(stringResource(R.string.wizard_pwm_summary, it.index + 1, it.pwm, it.flowMlMin))
            }
        }
        Button(onClick = { SprayerController.clearWizard(); onClose() }, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.action_done))
        }
    }
}

@Composable
private fun Failed(w: WizardState, onClose: () -> Unit) {
    StepCard(title = stringResource(R.string.wizard_not_completed)) {
        Text(stringResource(R.string.wizard_nothing_changed))
        Button(onClick = { SprayerController.clearWizard(); onClose() }, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.action_close))
        }
    }
}

// ---------------------------------------------------------------- pieces

@Composable
private fun StepCard(title: String, content: @Composable () -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            content()
        }
    }
}

@Composable
private fun BigValue(label: String, value: String, unit: String) {
    Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
        Text(label, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, fontSize = 44.sp, fontWeight = FontWeight.Black)
        Text(unit, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
