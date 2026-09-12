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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.meijworks.loofdoes.SprayerState
import com.meijworks.loofdoes.protocol.WizardMath
import com.meijworks.loofdoes.service.SprayerController
import com.meijworks.loofdoes.service.WizardMode
import com.meijworks.loofdoes.service.WizardState
import com.meijworks.loofdoes.service.WizardStep

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
                        Icon(Icons.Filled.Close, contentDescription = "Cancel")
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
                Text("Starting…")
            } else {
                Progress(w)
                when (w.step) {
                    WizardStep.STARTING -> Waiting("Taking calibration on the board…")
                    WizardStep.DOSE_CAPTURE -> DoseCapture(w, sprayer)
                    WizardStep.DOSE_ENTER -> DoseEnter(w)
                    WizardStep.PUMP_FIND -> PumpFind(w, sprayer)
                    WizardStep.PUMP_STEP_READY -> PumpStepReady(w)
                    WizardStep.PUMP_RUNNING -> PumpRunning(w)
                    WizardStep.PUMP_ENTER -> PumpEnter(w)
                    WizardStep.SAVING -> Waiting("Saving on the board…")
                    WizardStep.DONE -> Done(w, onClose)
                    WizardStep.FAILED -> Failed(w, onClose)
                }
                w.message?.let {
                    Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.fillMaxWidth(), textAlign = TextAlign.Center)
                }
                if (!sprayer.connected && w.step != WizardStep.FAILED && w.step != WizardStep.DONE) {
                    Text("Link lost", color = MaterialTheme.colorScheme.error)
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
        title = if (w.mode == WizardMode.DOSE_SINGLE) "Knob position ${w.doseIndex + 1}"
        else "Knob position ${w.doseIndex + 1} of ${WizardMath.DOSE_POINTS}",
    ) {
        Text("Set the knob to the ${w.doseLabel} position, then capture.")
        BigValue(label = "Knob reading", value = raw?.toString() ?: "–", unit = "of 4095")
        Button(
            onClick = { SprayerController.wizardCaptureDose(raw) },
            enabled = !w.busy && raw != null,
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Capture") }
    }
}

@Composable
private fun DoseEnter(w: WizardState) {
    var text by rememberSaveable(w.doseIndex) { mutableStateOf("") }
    val value = text.toIntOrNull()
    StepCard(title = "Dose for the ${w.doseLabel} position") {
        Text("Knob reading captured: ${w.capturedAnalog}")
        OutlinedTextField(
            value = text,
            onValueChange = { text = it.filter { c -> c.isDigit() } },
            label = { Text("Dose (l/ha)") },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { value?.let { SprayerController.wizardEnterDose(it) } },
            enabled = !w.busy && value != null && value > 0,
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Save point ${w.doseIndex + 1}") }
    }
}

// ---------------------------------------------------------------- pump

@Composable
private fun PumpFind(w: WizardState, sprayer: SprayerState) {
    StepCard(title = "Find the pump's start point") {
        Text("Slide up slowly until the pump just starts flowing, then capture.")
        BigValue(label = "Pump duty", value = w.findDuty.toString(), unit = "of 4095")
        Slider(
            value = w.findDuty.toFloat(),
            onValueChange = { SprayerController.wizardSetFindDuty(it.toInt()) },
            valueRange = 0f..WizardMath.MAX_DUTY.toFloat(),
            enabled = !w.busy,
        )
        Text(
            "Board reports pump duty ${sprayer.status?.pumpPwm ?: "–"}",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Button(
            onClick = { SprayerController.wizardCaptureStart() },
            enabled = !w.busy,
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Pump is flowing: capture") }
    }
}

@Composable
private fun PumpStepReady(w: WizardState) {
    StepCard(title = "Point ${w.pwmIndex + 1} of ${w.pwmSteps.size}") {
        Text("Put the measuring jug in place. The pump runs for exactly one minute at this duty; the board times it and stops on its own.")
        BigValue(label = "Pump duty", value = w.currentPwmDuty.toString(), unit = "of 4095")
        Button(
            onClick = { SprayerController.wizardStartRun() },
            enabled = !w.busy,
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Start 1-minute run") }
    }
}

@Composable
private fun PumpRunning(w: WizardState) {
    StepCard(title = "Point ${w.pwmIndex + 1} of ${w.pwmSteps.size}: running") {
        BigValue(label = "Remaining", value = (w.secondsRemaining ?: 0).toString(), unit = "s")
        LinearProgressIndicator(
            progress = { 1f - (w.secondsRemaining ?: 0).toFloat() / WizardMath.RUN_SECONDS },
            modifier = Modifier.fillMaxWidth(),
        )
        Text(
            "The screen may go dark; the run continues on the board.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        OutlinedButton(onClick = { SprayerController.wizardStopRun() }, modifier = Modifier.fillMaxWidth()) {
            Text("Stop and redo this point")
        }
    }
}

@Composable
private fun PumpEnter(w: WizardState) {
    var text by rememberSaveable(w.pwmIndex) { mutableStateOf("") }
    val value = text.toIntOrNull()
    StepCard(title = "Point ${w.pwmIndex + 1} of ${w.pwmSteps.size}: volume") {
        Text("Run complete at duty ${w.currentPwmDuty}. Enter the volume collected in the minute.")
        OutlinedTextField(
            value = text,
            onValueChange = { text = it.filter { c -> c.isDigit() } },
            label = { Text("Volume (ml)") },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { value?.let { SprayerController.wizardEnterVolume(it) } },
            enabled = !w.busy && value != null && value in 1..WizardMath.MAX_FLOW_ML_MIN,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(if (w.pwmIndex + 1 < w.pwmSteps.size) "Save and next point" else "Save and finish") }
    }
}

// ---------------------------------------------------------------- end

@Composable
private fun Done(w: WizardState, onClose: () -> Unit) {
    StepCard(title = "Saved on the board") {
        if (w.doseCaptured.isNotEmpty()) {
            Text("Knob positions", fontWeight = FontWeight.SemiBold)
            w.doseCaptured.forEach { Text("  ${it.index + 1}: analog ${it.analog}, ${it.doseLha} l/ha") }
        }
        if (w.pwmCaptured.isNotEmpty()) {
            Text("Pump curve", fontWeight = FontWeight.SemiBold)
            w.pwmCaptured.forEach { Text("  ${it.index + 1}: duty ${it.pwm}, ${it.flowMlMin} ml/min") }
        }
        Button(onClick = { SprayerController.clearWizard(); onClose() }, modifier = Modifier.fillMaxWidth()) { Text("Done") }
    }
}

@Composable
private fun Failed(w: WizardState, onClose: () -> Unit) {
    StepCard(title = "Calibration not completed") {
        Text("Nothing was changed on the board beyond what was already saved.")
        Button(onClick = { SprayerController.clearWizard(); onClose() }, modifier = Modifier.fillMaxWidth()) { Text("Close") }
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
